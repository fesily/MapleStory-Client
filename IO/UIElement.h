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

#include "Components/Button.h"
#include "Components/Icon.h"

#include "../Graphics/Sprite.h"

#include <string>
#include <vector>

namespace ms
{
	// Base class for all types of user interfaces on screen.
	class UIElement
	{
	public:
		using UPtr = std::unique_ptr<UIElement>;

		enum Type
		{
			NONE,
			START,
			LOGIN,
			TOS,
			GENDER,
			WORLDSELECT,
			REGION,
			CHARSELECT,
			LOGINWAIT,
			RACESELECT,
			CLASSCREATION,
			SOFTKEYBOARD,
			LOGINNOTICE,
			LOGINNOTICE_CONFIRM,
			STATUSMESSENGER,
			STATUSBAR,
			CHATBAR,
			BUFFLIST,
			NOTICE,
			NPCTALK,
			SHOP,
			STATSINFO,
			ITEMINVENTORY,
			EQUIPINVENTORY,
			SKILLBOOK,
			QUESTLOG,
			WORLDMAP,
			USERLIST,
			MINIMAP,
			CHANNEL,
			CHAT,
			CHATRANK,
			JOYPAD,
			EVENT,
			KEYCONFIG,
			OPTIONMENU,
			QUIT,
			CHARINFO,
			CASHSHOP,
			REVIVE,
			NUM_TYPES
		};

		virtual ~UIElement() {}

		// One thing an element offers to a caller which drives it instead of a player:
		// the name of a value it takes, the name of an action it runs, or the id of one
		// of its buttons. 'hint' tells what the values of a field look like where the
		// element can, e.g. the worlds a 'world' field accepts.
		struct Offer
		{
			enum class Kind
			{
				FIELD,
				ACTION,
				BUTTON
			};

			Kind kind;
			std::string name;
			std::string hint;
		};

		// The two calls an element answers to when it is driven, e.g. by a console
		// command or a script. A name the element does not have is answered with false,
		// which is what the default says: most elements take neither a value nor an
		// action. The value a field takes is the element's own to check, so the rules
		// (which names a character has, how long a PIC is) stay where the list is.
		virtual bool set_field(const std::string& name, const std::string& value) { return false; }
		virtual bool trigger(const std::string& action) { return false; }
		// Lists what the element offers, for a caller which reports what can be done
		// with the screen in front. The default lists the buttons, which is every
		// screen that has nothing but buttons.
		virtual void describe(std::vector<Offer>& out) const;

		// Press the button with that id, the way a click on it would. False when the
		// element answered that it has no such button.
		bool press_button(uint16_t id);

		virtual void draw(float inter) const;
		virtual void update();
		virtual void update_screen(int16_t new_width, int16_t new_height) {}

		void makeactive();
		void deactivate();
		bool is_active() const;

		virtual void toggle_active();
		virtual Button::State button_pressed(uint16_t buttonid) { return Button::State::DISABLED; }
		virtual bool send_icon(const Icon& icon, Point<int16_t> cursor_position) { return true; }

		virtual void doubleclick(Point<int16_t> cursorpos) {}
		virtual void rightclick(Point<int16_t> cursorpos) {}
		virtual bool is_in_range(Point<int16_t> cursor_position) const;
		virtual void remove_cursor();
		virtual Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_position);
		virtual void send_scroll(double yoffset) {}
		virtual void send_key(int32_t keycode, bool pressed, bool escape) {}

		virtual UIElement::Type get_type() const = 0;

	protected:
		UIElement(Point<int16_t> position, Point<int16_t> dimension, bool active);
		UIElement(Point<int16_t> position, Point<int16_t> dimension);
		UIElement();

		void draw_sprites(float alpha) const;
		void draw_buttons(float alpha) const;

		std::map<uint16_t, std::unique_ptr<Button>> buttons;
		std::vector<Sprite> sprites;
		Point<int16_t> position;
		Point<int16_t> dimension;
		bool active;
	};
}