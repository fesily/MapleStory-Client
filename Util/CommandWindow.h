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
#pragma once

#include <string>

namespace ms
{
	// The console: the lines the client writes to its output stream and the commands
	// that were entered, in one transcript, with the field the next command is typed
	// in, which completes like one of an editor: it lists what could still be typed
	// while one is, the arrows pick from that list and tab takes the pick. It is
	// drawn with ImGui, so it can be dragged out of the game window into one of its
	// own. The log window next to it shows the other stream (Util/Log.h, which writes
	// to the error stream), so a session can be redirected into one file and its
	// commands into another.
	namespace console_window
	{
		// Collect what the client writes to std::cout: the text still reaches the
		// console the client was started in, and a copy of it is kept in lines for
		// the window. Called before the client writes its first line.
		void attach_output();
		// Append a line to the transcript; the command reader echoes what it runs
		void append(const std::string& line);
		// Drop the transcript
		void clear();
		// Show or hide the window; the console command 'console' uses this
		void set_visible(bool visible);
		bool visible();

		// Draw the window of the current ImGui frame
		void draw();
	}
}
