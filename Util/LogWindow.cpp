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
	// The levels the window offers, in the order it lists them. The levels above
	// LOG_LEVEL never reach the sink, so they are not offered.
	const int OFFERED[] = { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG, LOG_NETWORK };
	const size_t OFFERED_COUNT = sizeof(OFFERED) / sizeof(OFFERED[0]);

	bool windowshown = true;
	bool levelshown[LOG_TRACE + 1] = { false, true, true, true, true, true, false, false };
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

	ImVec4 level_color(int level)
	{
		switch (level)
		{
			case LOG_ERROR:
				return ImVec4(1.00f, 0.42f, 0.42f, 1.00f);
			case LOG_WARN:
				return ImVec4(0.98f, 0.75f, 0.28f, 1.00f);
			case LOG_INFO:
				return ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
			case LOG_NETWORK:
				return ImVec4(0.55f, 0.80f, 0.95f, 1.00f);
			case LOG_UI:
				return ImVec4(0.78f, 0.72f, 0.96f, 1.00f);
			case LOG_TRACE:
				return ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
		}

		return ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
	}

	// A row shows one line, so the newlines a message carries (a dialog, a dump)
	// become spaces here; 'Copy' hands out the text the way it was written
	void draw_row(const ms::log::Entry& entry)
	{
		char clock[13];

		ms::log::format_clock(entry.wall_ms, clock, sizeof(clock));

		const std::string* shown = &entry.text;
		std::string flattened;

		if (entry.text.find_first_of("\r\n") != std::string::npos)
		{
			flattened = entry.text;

			for (char& c : flattened)
				if (c == '\r' || c == '\n')
					c = ' ';

			shown = &flattened;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, level_color(entry.level));
		ImGui::Text("%s [%s] %s", clock, ms::log::level_name(entry.level), shown->c_str());
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
			if (ordinal >= scanned && levelshown[entry.level] && contains_ignoring_case(entry.text, needle))
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
			if (!levelshown[entry.level] || !contains_ignoring_case(entry.text, needle))
				continue;

			char clock[13];

			ms::log::format_clock(entry.wall_ms, clock, sizeof(clock));

			all += std::string("[") + clock + "] [" + ms::log::level_name(entry.level) + "] " + entry.text + '\n';
		}

		if (!all.empty())
			ImGui::SetClipboardText(all.c_str());
	}

	void draw_toolbar(const ms::log::Locked& locked)
	{
		// One checkbox per level, so a noisy one can be switched off while the rest
		// stays in sight, in the color its rows are drawn in
		for (size_t i = 0; i < OFFERED_COUNT; i++)
		{
			int level = OFFERED[i];

			if (i > 0)
				ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, level_color(level));
			ImGui::Checkbox(ms::log::level_name(level), &levelshown[level]);
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();
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
