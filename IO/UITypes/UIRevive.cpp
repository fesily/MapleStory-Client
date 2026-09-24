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
#include "UIRevive.h"

#include "../../Constants.h"

#include "../../Gameplay/Movement.h"

#include "../KeyAction.h"

#include "../../Net/Packets/GameplayPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	// UIWindow2.img/Notice holds the boxes the original client opens for a dead player:
	// "1" reads "PRESS OK TO BE REVIVED" and "2" is the Wheel of Destiny variant. The
	// window of UIRevive.cpp in the 095 client is built from the same nodes.
	UIRevive::UIRevive()
	{
		nl::node Notice = nl::nx::UI["UIWindow2.img"]["Notice"];
		nl::node backgrnd = Notice["1"];
		nl::node okbutton = Notice["btOK"];

		sprites.emplace_back(backgrnd);

		dimension = Texture(backgrnd).get_dimensions();

		// The box art leaves an empty plate at the bottom for the button (300x131 box,
		// plate at y 96..119), the button art carries nothing but its own origin: buttons
		// draw at position - origin (MapleButton::bounds), so the anchor that lands the
		// button centered on the plate is that plate position plus the origin.
		Texture oktexture = okbutton["normal"]["0"];
		Point<int16_t> oksize = oktexture.get_dimensions();
		Point<int16_t> okplate = Point<int16_t>(dimension.x() / 2 - oksize.x() / 2, 100);

		buttons[Buttons::OK] = std::make_unique<MapleButton>(okbutton, okplate + oktexture.get_origin());

		update_screen(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight());
	}

	void UIRevive::update_screen(int16_t new_width, int16_t new_height)
	{
		position = Point<int16_t>(new_width / 2 - dimension.x() / 2, new_height / 2 - dimension.y() / 2);
	}

	void UIRevive::send_key(int32_t keycode, bool pressed, bool escape)
	{
		// The dialog is the only way out of the dead state, so escape does not close it.
		if (pressed && keycode == KeyAction::RETURN)
			revive();
	}

	UIElement::Type UIRevive::get_type() const
	{
		return TYPE;
	}

	Button::State UIRevive::button_pressed(uint16_t buttonid)
	{
		if (buttonid == Buttons::OK)
			revive();

		return Button::State::NORMAL;
	}

	void UIRevive::revive()
	{
		// UIRevive::Revive of the 095 client calls
		// CField::SendTransferFieldRequest(0, "", premium, 0, 0), which is this request
		// with the dying flag and without the Wheel of Destiny.
		ChangeMapPacket(true, 0, "", false).dispatch();
	}
}
