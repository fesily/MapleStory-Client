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

namespace ms
{
	// The debug window that shows what the log sink (Util/Log.h) keeps: the lines of
	// the levels that are switched on, narrowed down by a text, followed to the
	// bottom while that is wanted. It is drawn with ImGui, so it can be dragged out
	// of the game window into one of its own.
	namespace log_window
	{
		// Whether the window is shown; the console command 'log' switches it
		void set_visible(bool visible);
		bool visible();

		// Draw the window of the current ImGui frame
		void draw();
	}
}
