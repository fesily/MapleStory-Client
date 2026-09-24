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
#include "DebugUI.h"

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

	// The commands the hint lists and which of them is picked: the arrows walk the
	// pick and tab takes it. The name it was worked out for is kept, so that typing
	// starts the pick over instead of leaving it where it was
	std::string hintname;
	int32_t hintpick = 0;

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

	// The name the hint is worked out for: what was typed before the first space,
	// with '?' standing for the name the console runs it as
	std::string hint_name(const char* typed)
	{
		std::string name = typed;
		size_t space = name.find_first_of(" \t");

		if (space != std::string::npos)
			name.erase(space);

		if (name == "?")
			name = "help";

		return name;
	}

	// Whether the hint lists the command under that name, which is what the keys
	// that walk it work on as well
	bool hinted(const std::string& name, const ms::debug_console::Command& command)
	{
		return command.name.compare(0, name.size(), name) == 0;
	}

	// How many commands the hint lists, in the order 'help' lists them
	size_t hint_size(const std::string& name)
	{
		if (name.empty())
			return 0;

		size_t size = 0;

		for (const ms::debug_console::Command& command : ms::debug_console::command_list())
			if (hinted(name, command))
				size++;

		return size;
	}

	// The command the given position of the hint holds
	const ms::debug_console::Command* hint_command(const std::string& name, int32_t picked)
	{
		int32_t position = 0;

		for (const ms::debug_console::Command& command : ms::debug_console::command_list())
		{
			if (!hinted(name, command))
				continue;

			if (position == picked)
				return &command;

			position++;
		}

		return nullptr;
	}

	// Make the pick fit the name that is typed, which it belongs to: it starts on
	// the command the name is complete as, which is the one that runs, or on the
	// first one the name is the beginning of
	void hint_fit(const std::string& name)
	{
		if (name == hintname)
			return;

		hintname = name;
		hintpick = 0;

		int32_t position = 0;

		for (const ms::debug_console::Command& command : ms::debug_console::command_list())
		{
			if (!hinted(name, command))
				continue;

			if (command.name == name)
			{
				hintpick = position;

				break;
			}

			position++;
		}
	}

	// Walk the lines that were entered before, the way the console of Dear ImGui's
	// own demo does it: one step per press, the oldest at the top
	void walk_history(ImGuiInputTextCallbackData* data)
	{
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
			return;

		const char* line = historyindex >= 0 ? history[static_cast<size_t>(historyindex)].c_str() : "";

		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, line);
	}

	// The keys the field takes care of itself, which is what makes it complete like
	// one of an editor: the arrows walk the hint while it lists more than one command
	// and the lines that were entered before when it does not, and tab takes the
	// command the hint is on
	int input_callback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
		{
			std::string name = hint_name(data->Buf);
			size_t size = hint_size(name);

			// One command is nothing to walk and none of them is what is typed, so the
			// arrows are free to walk the lines that were entered before
			if (size < 2)
			{
				walk_history(data);

				return 0;
			}

			hint_fit(name);

			if (data->EventKey == ImGuiKey_UpArrow)
				hintpick = hintpick > 0 ? hintpick - 1 : static_cast<int32_t>(size) - 1;
			else
				hintpick = (hintpick + 1) % static_cast<int32_t>(size);

			return 0;
		}

		if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
		{
			// Tab takes the command the hint is on: the typed name is what it replaces,
			// what follows it are the arguments and stay as they are, and a command
			// that takes arguments keeps a space for them
			std::string name = hint_name(data->Buf);

			hint_fit(name);

			const ms::debug_console::Command* command = hint_command(name, hintpick);

			if (!command)
				return 0;

			std::string text(data->Buf, static_cast<size_t>(data->BufTextLen));
			size_t end = text.find_first_of(" \t");

			if (end == std::string::npos)
				end = text.size();

			std::string taken = command->name;

			if (end == text.size() && !command->args.empty())
				taken += ' ';

			data->DeleteChars(0, static_cast<int>(end));
			data->InsertChars(0, taken.c_str());

			data->CursorPos = data->SelectionStart = data->SelectionEnd = static_cast<int>(taken.size());

			return 0;
		}

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

	// The hint under the field: the commands the typed name is the beginning of, with
	// the arguments they take and what they do, of which the arrows pick one and tab
	// takes it. The name that is complete is the one that would run, so the pick
	// starts on it and it is not dimmed. While a command waits for a line, the field
	// answers that instead, and the hint says how to drop it. It is a tooltip, so it
	// takes neither the mouse nor the keyboard from the field.
	void draw_hint(const char* typed)
	{
		std::string name = hint_name(typed);

		// While a command waits for a line the field answers it rather than naming a
		// command, and an empty field names nothing a hint could go on
		bool awaiting = ms::debug_console::is_awaiting();

		if (!awaiting && name.empty())
			return;

		// The hint hangs under the field it belongs to, wherever its window was put
		const ImVec2 corner = ImGui::GetItemRectMin();
		const float below = ImGui::GetItemRectMax().y + ImGui::GetStyle().ItemSpacing.y;

		ImGui::SetNextWindowPos(ImVec2(corner.x, below));

		if (!ImGui::BeginTooltip())
			return;

		if (awaiting)
		{
			ImGui::TextUnformatted("A command asked for this line; 'cancel' drops it.");

			ImGui::EndTooltip();

			return;
		}

		// The pick belongs to the name that is typed, so it is made to fit it before
		// it is drawn: what is typed decides where the keys that walk it start
		hint_fit(name);

		// Nothing begins with what was typed, which is the same the console itself
		// says of the name when the line is entered
		if (hint_size(name) == 0)
		{
			ImGui::Text("Unknown command: %s (type 'help')", name.c_str());

			ImGui::EndTooltip();

			return;
		}

		// Two columns, so the descriptions line up under each other; the table takes
		// its width from the longest signature, which is what sizes the tooltip
		if (ImGui::BeginTable("hint", 2, ImGuiTableFlags_SizingFixedFit))
		{
			int32_t position = 0;

			for (const ms::debug_console::Command& command : ms::debug_console::command_list())
			{
				if (!hinted(name, command))
					continue;

				// The one the pick is on is the one tab would take, so it is the one
				// that stands out; what the name could still become is dimmed, the name
				// it stands for, which is the one that runs, is not
				bool picked = position == hintpick;
				bool runs = command.name == name;

				if (picked)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));

				if (!runs && !picked)
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

				std::string signature = command.name;

				if (!command.args.empty())
				{
					signature += ' ';
					signature += command.args;
				}

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(signature.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(command.description.c_str());

				if (!runs && !picked)
					ImGui::PopStyleColor();

				position++;
			}

			ImGui::EndTable();
		}

		ImGui::EndTooltip();
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
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion,
			input_callback
		);

		// The field hints at what could be typed while it is used: focused, or under
		// the mouse which is about to focus it
		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			draw_hint(line);

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

			// The first time it is opened the console keeps a size of its own, at the
			// scale the debug windows are drawn at, and is placed inside the game window
			// like the log window next to it; where it was dragged to afterwards is
			// what it is opened at
			float scaling = ms::debugui::scale();

			ImGui::SetNextWindowSize(ImVec2(600.0f * scaling, 220.0f * scaling), ImGuiCond_FirstUseEver);

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
