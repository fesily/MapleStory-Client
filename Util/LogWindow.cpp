//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License		//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "LogWindow.h"

#include "../MapleStory.h"
#include "DebugUI.h"
#include "Log.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <deque>
#include <string>
#include <vector>

namespace
{
	// The severities and the channels the window offers, in the order it lists
	// them. A line is drawn when both of the two it has are switched on.
	const int OFFEREDSEVERITIES[] = {
		spdlog::level::err,
		spdlog::level::warn,
		spdlog::level::info,
		spdlog::level::debug,
		spdlog::level::trace
	};
	const size_t SEVERITYCOUNT = sizeof(OFFEREDSEVERITIES) / sizeof(OFFEREDSEVERITIES[0]);

	const ms::log::Channel OFFEREDCHANNELS[] = {
		ms::log::Channel::CLIENT,
		ms::log::Channel::NETWORK,
		ms::log::Channel::UI
	};
	const size_t CHANNELCOUNT = sizeof(OFFEREDCHANNELS) / sizeof(OFFEREDCHANNELS[0]);

	bool windowshown = true;
	// Indexed by the severity a line was written with. The trace lines only reach
	// the sink when the log is put on the trace level, so they may be switched on
	// and still show nothing.
	bool severityshown[spdlog::level::n_levels] = { true, true, true, true, true, true, false };
	// The lines the network and the ui wrote are out of sight until they are asked
	// for: they are the noisiest of the three channels
	bool channelshown[static_cast<size_t>(ms::log::Channel::COUNT)] = { true, true, false };
	char filtertext[64] = {};
	bool follow = true;

	// What 'matches' was built from: the filter it was narrowed down with, how many
	// lines of the buffer were looked at and how many were dropped in front of them
	// then. While lines are only added at the back, the ones that were looked at
	// stay where they are and only the new ones have to be tested.
	std::vector<size_t> matches;
	std::string builtfilter;
	size_t scanned = 0;
	size_t builtoffset = 0;

	bool contains_ignoring_case(const std::string& line, const std::string& needle)
	{
		if (needle.empty())
			return true;

		auto equal_ignoring_case = [](char left, char right)
		{
			return std::tolower(static_cast<unsigned char>(left)) == right;
		};

		return std::search(line.begin(), line.end(), needle.begin(), needle.end(), equal_ignoring_case) != line.end();
	}

	std::string lowercased(const char* text)
	{
		std::string lowered = text;

		for (char& c : lowered)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		return lowered;
	}

	// The color of a row: the channel decides for the lines the network and the ui
	// wrote, the severity for a client line, which is how the window colored the
	// rows before the two were channels
	ImVec4 line_color(int severity, ms::log::Channel channel)
	{
		switch (channel)
		{
			case ms::log::Channel::NETWORK:
				return ImVec4(0.55f, 0.80f, 0.95f, 1.00f);
			case ms::log::Channel::UI:
				return ImVec4(0.78f, 0.72f, 0.96f, 1.00f);
			case ms::log::Channel::CLIENT:
				break;
		}

		switch (severity)
		{
			case spdlog::level::err:
				return ImVec4(1.00f, 0.42f, 0.42f, 1.00f);
			case spdlog::level::warn:
				return ImVec4(0.98f, 0.75f, 0.28f, 1.00f);
			case spdlog::level::info:
				return ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
			case spdlog::level::trace:
				return ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
		}

		return ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
	}

	// Whether the switches of the toolbar let a line through
	bool line_shown(const ms::log::Entry& entry)
	{
		return severityshown[entry.level] && channelshown[static_cast<size_t>(entry.channel)];
	}

	// A row shows one line, so the newlines a message carries (a dialog, a dump)
	// become spaces here; 'Copy' hands out the text the way it was written
	void draw_row(const ms::log::Entry& entry)
	{
		char clock[13];

		ms::log::format_clock(entry.wall_ms, clock, sizeof(clock));

		const std::string* rowtext = &entry.text;
		std::string flattened;

		if (entry.text.find_first_of("\r\n") != std::string::npos)
		{
			flattened = entry.text;

			for (char& c : flattened)
				if (c == '\r' || c == '\n')
					c = ' ';

			rowtext = &flattened;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, line_color(entry.level, entry.channel));
		ImGui::Text("%s [%s] %s", clock, ms::log::line_tag(entry.level, entry.channel), rowtext->c_str());
		ImGui::PopStyleColor();
	}

	// Test the lines that were not looked at yet and keep the ordinals of the ones
	// the filter keeps
	void build_matches(const ms::log::Locked& locked)
	{
		const std::deque<ms::log::Entry>& lines = locked.lines();
		std::string needle = lowercased(filtertext);

		if (needle != builtfilter || locked.dropped() != builtoffset || lines.size() < scanned)
		{
			matches.clear();

			scanned = 0;
			builtfilter = needle;
			builtoffset = locked.dropped();
		}

		if (scanned == lines.size())
			return;

		size_t ordinal = 0;

		for (const ms::log::Entry& entry : lines)
		{
			// The lines that were looked at before only pass by; the filter runs on
			// the ones that are new
			if (ordinal >= scanned && line_shown(entry) && contains_ignoring_case(entry.text, needle))
				matches.push_back(ordinal);

			ordinal++;
		}

		scanned = lines.size();
	}

	void copy_lines(const ms::log::Locked& locked)
	{
		std::string needle = lowercased(filtertext);
		std::string all;

		for (const ms::log::Entry& entry : locked.lines())
		{
			if (!line_shown(entry) || !contains_ignoring_case(entry.text, needle))
				continue;

			char clock[13];

			ms::log::format_clock(entry.wall_ms, clock, sizeof(clock));

			all += std::string("[") + clock + "] [" + ms::log::line_tag(entry.level, entry.channel) + "] " + entry.text + '\n';
		}

		if (!all.empty())
			ImGui::SetClipboardText(all.c_str());
	}

	void draw_toolbar(const ms::log::Locked& locked)
	{
		// One checkbox per severity and one per channel, so a noisy part of the log
		// can be switched off while the rest stays in sight, in the color its rows
		// are drawn in
		for (size_t i = 0; i < SEVERITYCOUNT; i++)
		{
			int severity = OFFEREDSEVERITIES[i];

			if (i > 0)
				ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, line_color(severity, ms::log::Channel::CLIENT));
			ImGui::Checkbox(ms::log::severity_name(severity), &severityshown[severity]);
			ImGui::PopStyleColor();
		}

		for (size_t i = 0; i < CHANNELCOUNT; i++)
		{
			ms::log::Channel channel = OFFEREDCHANNELS[i];

			if (i > 0)
				ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, line_color(spdlog::level::info, channel));
			ImGui::Checkbox(ms::log::channel_name(channel), &channelshown[static_cast<size_t>(channel)]);
			ImGui::PopStyleColor();
		}

		ImGui::Checkbox("Follow", &follow);

		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputTextWithHint("##filter", "filter", filtertext, IM_ARRAYSIZE(filtertext));

		ImGui::SameLine();

		if (ImGui::Button("Copy"))
			copy_lines(locked);

		ImGui::SameLine();

		if (ImGui::Button("Clear"))
			ms::log::clear();

		ImGui::SameLine();
		ImGui::TextDisabled(
			"%zu of %zu lines kept, %zu dropped before them",
			matches.size(),
			locked.lines().size(),
			locked.dropped()
		);
	}

	void draw_rows(const ms::log::Locked& locked)
	{
		const std::deque<ms::log::Entry>& lines = locked.lines();

		if (matches.empty())
		{
#ifdef _DEBUG
			ImGui::TextDisabled(lines.empty() ? "Nothing was logged yet." : "No line matches the filter.");
#else
			ImGui::TextDisabled("A release build compiles LOG() out, so nothing reaches this window.");
#endif
			return;
		}

		// Scrolling up leaves the newest line, so following has to be switched on
		// again after it
		if (follow && ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel > 0.0f)
			follow = false;

		ImGuiListClipper clipper;

		// The clipper measures by the height of a row and every row is one line, so
		// wrapping is off and long lines are read by scrolling sideways
		ImGui::PushTextWrapPos(FLT_MAX);

		clipper.Begin(static_cast<int>(matches.size()));

		while (clipper.Step())
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				draw_row(lines[matches[static_cast<size_t>(row)]]);

		ImGui::PopTextWrapPos();

		if (follow)
			ImGui::SetScrollY(ImGui::GetScrollMaxY());
	}
}

namespace ms
{
	namespace log_window
	{
		void set_visible(bool visible)
		{
			windowshown = visible;
		}

		bool visible()
		{
			return windowshown;
		}

		void draw()
		{
			if (!windowshown)
				return;

			// The first time it is opened the window is sized to hold the same lines at
			// the scale the debug windows are drawn at; afterwards it is where it was
			// dragged and resized to
			float scaling = debugui::scale();

			ImGui::SetNextWindowSize(ImVec2(600.0f * scaling, 300.0f * scaling), ImGuiCond_FirstUseEver);

			bool open = true;

			if (ImGui::Begin("Log", &open))
			{
				// The buffer is locked while it is drawn, which is what makes
				// addressing its lines by an ordinal collected a moment ago safe
				log::Locked locked;

				build_matches(locked);
				draw_toolbar(locked);
				// The toolbar can clear the buffer, which invalidates the ordinals
				// 'matches' was built from
				build_matches(locked);

				// The rows scroll inside the window, so the toolbar above them stays
				// in sight while they follow the newest line
				if (ImGui::BeginChild("rows", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
					draw_rows(locked);

				ImGui::EndChild();
			}

			ImGui::End();

			windowshown = open;
		}
	}
}
