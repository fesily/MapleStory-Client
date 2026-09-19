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
#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace ms
{
	// The console window the client is started with reads commands: the lines are
	// collected by a thread of its own, so that the loop never waits for input, and
	// poll() runs them on the game thread, where they can touch the game state
	// without locking it.
	namespace debug_console
	{
		// A command that 'help' lists and poll() runs
		struct Command
		{
			std::string name;
			std::string args;
			std::string description;
			std::function<void(const std::string&)> handler;
		};

		// Start collecting the lines entered on standard input
		void start();
		// Hand a line over as if it had been entered on standard input, which is how
		// the console window (Util/CommandWindow.h) enters one
		void submit(const std::string& line);
		// Run the commands entered since the last call
		void poll();
		// Add commands, e.g. from a table in the file which owns the game loop
		void add(std::initializer_list<Command> commands);
		// The commands in the order they were added, which is what the console window
		// hints at while one is typed (Util/CommandWindow.h)
		const std::vector<Command>& command_list();
		// Hand the next line entered to a handler instead of reading it as a command,
		// which is how a command asks for the values it needs. The line arrives without
		// the whitespace around it; an empty line is handed over like any other, so a
		// prompt can repeat itself, and 'cancel' drops the handler instead of calling it
		// (which is_awaiting() then reports to whoever asked).
		void await_line(std::function<void(const std::string&)> handler);
		// Stop handing the lines entered to the handler of await_line()
		void cancel_await();
		// Whether the lines entered still go to a handler
		bool is_awaiting();
	}
}
