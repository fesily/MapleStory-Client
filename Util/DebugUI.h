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

struct GLFWwindow;

namespace ms
{
	// The debug windows the client draws with Dear ImGui, and what the game asks
	// about them: input a window of them takes must not reach the game.
	namespace debugui
	{
		// Create the ImGui context and load the font it prints with; no window is
		// needed for it
		void init();
		// Bind the ImGui backends to the window the game draws in. The window is
		// destroyed and created again whenever the screen mode changes, so this runs
		// again for every window.
		void attach(GLFWwindow* window);
		// Take the backends down while the window they are bound to still exists; the
		// game calls this before it destroys that window, because the backends restore
		// its callbacks on the way out and would write into a window that is gone
		void detach();
		// Build the windows of this frame and draw them; runs after the game has
		// drawn and before the buffers are swapped
		void draw();

		// Whether a debug window takes the mouse or the keyboard this frame
		bool captures_mouse();
		bool captures_keyboard();

		// Show or hide the log window; the console command 'log' uses this
		void set_log_visible(bool visible);
		void toggle_log();
		// Show or hide the console window; the console command 'console' uses this
		void set_console_visible(bool visible);
	}
}
