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
	// Drives the three screens of the login from the console: it fills in the account
	// and the password, picks a world with one of its channels and picks a character,
	// by pressing the same buttons a click presses. The screens are drawn and animated
	// the way they always are, the script only replaces the mouse.
	//
	// The calls it makes are the ones every element answers to (UIElement::set_field
	// and UIElement::trigger), so the script names the screens it works by their element
	// type and does not know their classes.
	namespace login_script
	{
		// The 'login', 'world', 'char' and 'cancel' commands
		void register_commands();
		// Carry out the step the screen in front asks for, called once per frame
		void tick();
		// Stop the script and give the mouse back to the player
		void cancel();
	}
}
