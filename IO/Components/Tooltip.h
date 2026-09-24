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

#include "../UIElement.h"
#include "../../Template/Point.h"

namespace ms
{
	// Interface for tooltips
	// Window with helpful information that appears on mouse hover at a specific location
	// The parent a tooltip belongs to is named with the UIElement::Type of that window: a second
	// enum of its own only meant the two had to be translated by hand, since their values never
	// matched, and comparing them directly is deprecated in C++20 (C5054).
	class Tooltip
	{
	public:
		virtual ~Tooltip() {}

		virtual void draw(Point<int16_t> cursorpos) const = 0;
	};
}