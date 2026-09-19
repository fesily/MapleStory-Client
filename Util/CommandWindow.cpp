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
#include "CommandWindow.h"

#include "DebugConsole.h"

#include "imgui.h"

#include <cfloat>
#include <deque>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>
#include <vector>

namespace
{
	// The transcript: the lines the client printed and the commands that were run,
	// the newest at the back. It is trimmed, so the client does not grow with what
	// its own output.
	std::deque<std::string> transcript;
	std::recursive_mutex transcriptmutex;
	const size_t MAX_LINES = 2000;

	// The text printed since the last newline; a line is often printed in pieces
	std::string partial;

	// Whether the transcript follows the newest line or the reader scrolled back
	bool following = true;

	bool windowshown = true;

	// The lines entered before, so one of them can be called back with the arrows
	std::vector<std::string> history;
	int32_t historyindex = -1;

	void add_line(const std::string& line)
	{
		transcript.push_back(line);

		while (transcript.size() > MAX_LINES)
			transcript.pop_front();
	}

	// The stream the client wrote to before it was collected, so its text still
	// reaches the console it was started in
	std::streambuf* consoleoutput = nullptr;

	void collect(const char* text, size_t length)
	{
		std::lock_guard<std::recursive_mutex> guard(transcriptmutex);

		for (size_t index = 0; index < length; index++)
		{
			char character = text[index];

			if (character == '\r')
				continue;

			if (character != '\n')
			{
				partial += character;
				continue;
			}

			add_line(partial);

			partial.clear();
		}
	}

	// The output stream is written as before and the same text is collected in lines
	// for the window: collecting it here is what makes every print show up in the
	// transcript without the code that prints having to know about the window
	class TeeBuffer : public std::streambuf
	{
	public:
		void set_target(std::streambuf* buffer)
		{
			target = buffer;
		}

	protected:
		int_type overflow(int_type character) override
		{
			// An end of file asks for a flush, which is not a character to collect
			if (traits_type::eq_int_type(character, traits_type::eof()))
				return traits_type::not_eof(character);

			char text = traits_type::to_char_type(character);

			if (traits_type::eq_int_type(target->sputc(text), traits_type::eof()))
				return traits_type::eof();

			collect(&text, 1);

			return traits_type::not_eof(character);
		}

		std::streamsize xsputn(const char* text, std::streamsize length) override
		{
			std::streamsize written = target->sputn(text, length);

			collect(text, static_cast<size_t>(written));

			return written;
		}

		int sync() override
		{
			return target->pubsync();
		}

	private:
		std::streambuf* target = nullptr;
	};

	TeeBuffer teebuffer;

	// Walk the history with the arrow keys, the way the console of Dear ImGui's own
	// demo does it
	int input_callback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory)
			return 0;

		int32_t previous = historyindex;

		if (data->EventKey == ImGuiKey_UpArrow)
		{
			if (historyindex < 0)
				historyindex = static_cast<int32_t>(history.size()) - 1;
			else if (historyindex > 0)
				historyindex--;
		}
		else if (data->EventKey == ImGuiKey_DownArrow)
		{
			if (historyindex >= 0 && static_cast<size_t>(historyindex) + 1 < history.size())
				historyindex++;
			else
				historyindex = -1;
		}

		if (previous == historyindex)
			return 0;

		const char* line = historyindex >= 0 ? history[static_cast<size_t>(historyindex)].c_str() : "";

		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, line);

		return 0;
	}

	void draw_transcript()
	{
		std::lock_guard<std::recursive_mutex> guard(transcriptmutex);

		if (transcript.empty())
		{
			ImGui::TextDisabled("Nothing was printed yet; 'help' lists the commands.");

			return;
		}

		// Reading back through the transcript is not interrupted by the lines that
		// arrive while it is read; the newest line is followed again at the end
		if (following && ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel > 0.0f)
			following = false;
		else if (!following && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f)
			following = true;

		// One row per line and no wrapping, like the log window: the clipper that
		// keeps a long transcript cheap measures by the height of a row, and long
		// lines are read by scrolling sideways
		ImGuiListClipper clipper;

		ImGui::PushTextWrapPos(FLT_MAX);

		clipper.Begin(static_cast<int>(transcript.size()));

		while (clipper.Step())
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				ImGui::TextUnformatted(transcript[static_cast<size_t>(row)].c_str());

		ImGui::PopTextWrapPos();

		if (following)
			ImGui::SetScrollY(ImGui::GetScrollMaxY());
	}

	void draw_input()
	{
		static char line[256] = {};

		ImGui::SetNextItemWidth(-1.0f);

		bool submitted = ImGui::InputTextWithHint(
			"##command",
			"command ('help' lists them)",
			line,
			IM_ARRAYSIZE(line),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
			input_callback
		);

		if (!submitted)
			return;

		std::string entered = line;

		// An empty line is what a command which asked for a value may want, so it is
		// handed over like any other; the history keeps the ones that carry text
		if (!entered.empty())
		{
			history.push_back(entered);

			if (history.size() > 100)
				history.erase(history.begin());
		}

		historyindex = -1;

		line[0] = '\0';

		ms::debug_console::submit(entered);

		// Several commands in a row are what the window is for, so the field keeps
		// the keyboard until the game is clicked again
		ImGui::SetKeyboardFocusHere(-1);
	}
}

namespace ms
{
	namespace console_window
	{
		void attach_output()
		{
			std::lock_guard<std::recursive_mutex> guard(transcriptmutex);

			// Collecting the stream twice would send every line through the tee and
			// into the transcript again
			if (consoleoutput)
				return;

			consoleoutput = std::cout.rdbuf();

			teebuffer.set_target(consoleoutput);
			std::cout.rdbuf(&teebuffer);
		}

		void append(const std::string& line)
		{
			std::lock_guard<std::recursive_mutex> guard(transcriptmutex);

			// A line handed over while a print waits for its newline keeps the order
			// the lines were made in
			if (!partial.empty())
			{
				add_line(partial);

				partial.clear();
			}

			add_line(line);
		}

		void clear()
		{
			std::lock_guard<std::recursive_mutex> guard(transcriptmutex);

			transcript.clear();

			following = true;
		}

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

			// The first time it is opened the console keeps a size of its own and is
			// placed inside the game window like the log window next to it; where it
			// was dragged to afterwards is what it is opened at
			ImGui::SetNextWindowSize(ImVec2(600.0f, 220.0f), ImGuiCond_FirstUseEver);

			bool open = true;

			if (ImGui::Begin("Console", &open))
			{
				// The transcript scrolls on its own, so the field that is typed in
				// stays where it is
				float inputheight = ImGui::GetFrameHeightWithSpacing();

				if (ImGui::BeginChild("output", ImVec2(0.0f, -inputheight), false, ImGuiWindowFlags_HorizontalScrollbar))
					draw_transcript();

				ImGui::EndChild();

				draw_input();
			}

			ImGui::End();

			windowshown = open;
		}
	}
}
