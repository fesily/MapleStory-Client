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
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "DebugConsole.h"

#include "CommandWindow.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	const char* WHITESPACE = " \t\r\n";

	std::mutex inputmutex;
	std::vector<std::string> input;

	// The read waits until a line is entered, which the loop cannot do, so it runs on
	// a thread of its own. The thread is detached: it sits in that read until the
	// process ends, and the lines it appends are the only state it touches.
	void read_input()
	{
		std::string line;

		while (std::getline(std::cin, line))
		{
			std::lock_guard<std::mutex> guard(inputmutex);
			input.push_back(line);
		}
	}
}

namespace ms
{
	namespace debug_console
	{
		namespace
		{
			std::vector<Command> commands;

			// Set while the lines entered answer a prompt instead of naming a command
			std::function<void(const std::string&)> awaiting;

			// The line without the whitespace around it, which the reader leaves on it
			std::string trim(const std::string& line)
			{
				size_t begin = line.find_first_not_of(WHITESPACE);

				if (begin == std::string::npos)
					return std::string();

				size_t end = line.find_last_not_of(WHITESPACE);

				return line.substr(begin, end + 1 - begin);
			}

			// Split a line into the command name and the arguments behind it
			void split(const std::string& line, std::string& name, std::string& args)
			{
				size_t begin = line.find_first_not_of(WHITESPACE);

				if (begin == std::string::npos)
				{
					name.clear();
					args.clear();
					return;
				}

				size_t end = line.find_first_of(WHITESPACE, begin);

				if (end == std::string::npos)
					end = line.size();

				name = line.substr(begin, end - begin);

				size_t argbegin = line.find_first_not_of(WHITESPACE, end);
				size_t argend = line.find_last_not_of(WHITESPACE);

				args = argbegin == std::string::npos ? std::string() : line.substr(argbegin, argend + 1 - argbegin);
			}

			void list()
			{
				size_t width = 0;

				for (const Command& command : commands)
					width = std::max(width, command.name.size() + command.args.size() + 1);

				std::cout << "Commands:" << std::endl;

				for (const Command& command : commands)
					std::cout << "  " << std::left << std::setw(static_cast<int32_t>(width) + 2)
						<< (command.name + " " + command.args) << command.description << std::endl;
			}

			void describe(const std::string& name)
			{
				for (const Command& command : commands)
				{
					if (command.name != name)
						continue;

					std::cout << command.name << " " << command.args << ": " << command.description << std::endl;
					return;
				}

				std::cout << "Unknown command: " << name << std::endl;
			}

			void run(const std::string& line)
			{
				// The transcript of the console shows what was entered, whichever of
				// the two streams the line came in on
				console_window::append(std::string("> ") + line);

				// A command which asked for the next lines takes one exactly as it was
				// typed, so a name which is a command of its own can be entered and an
				// empty line can repeat what the prompt wants. 'cancel' is the way out.
				if (awaiting)
				{
					std::function<void(const std::string&)> handler = awaiting;
					awaiting = {};

					if (trim(line) == "cancel")
						std::cout << "Input cancelled." << std::endl;
					else
						handler(trim(line));

					return;
				}

				std::string name;
				std::string args;

				split(line, name, args);

				if (name.empty())
					return;

				// 'help' is part of the console itself: it is what lists the commands
				if (name == "help" || name == "?")
				{
					if (args.empty())
						list();
					else
						describe(args);

					return;
				}

				for (const Command& command : commands)
				{
					if (command.name != name)
						continue;

					command.handler(args);
					return;
				}

				std::cout << "Unknown command: " << name << " (type 'help')" << std::endl;
			}
		}

		void add(std::initializer_list<Command> list)
		{
			commands.insert(commands.end(), list.begin(), list.end());
		}

		void await_line(std::function<void(const std::string&)> handler)
		{
			awaiting = handler;
		}

		void cancel_await()
		{
			awaiting = {};
		}

		bool is_awaiting()
		{
			return awaiting != nullptr;
		}

		void start()
		{
			std::thread(read_input).detach();
		}

		void submit(const std::string& line)
		{
			std::lock_guard<std::mutex> guard(inputmutex);

			input.push_back(line);
		}

		void poll()
		{
			std::vector<std::string> lines;

			{
				std::lock_guard<std::mutex> guard(inputmutex);
				lines.swap(input);
			}

			for (const std::string& line : lines)
				run(line);
		}
	}
}
