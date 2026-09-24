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

#include "../Components/MapleButton.h"

namespace ms
{
	// Shown while the player's HP is zero. The server keeps a dead character in the map
	// until the client asks for a revival, which is a map change request with the dying
	// flag: ChangeMapHandler sends a dead player to the return map of the map they are on
	// whatever map the request names (ChangeMapHandler.java:98-115).
	class UIRevive : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::REVIVE;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIRevive();

		void update_screen(int16_t new_width, int16_t new_height) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void revive();

		enum Buttons : uint16_t
		{
			OK
		};
	};
}
