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
#include "UINpcTalk.h"

#include "../UI.h"

#include "../../Data/TextResolver.h"
#include "../../Configuration.h"
#include "../../MapleStory.h"

#include "../Components/MapleButton.h"

#include "UINotice.h"

#include "../../Gameplay/Stage.h"

#include "../../Net/Packets/NpcInteractionPackets.h"

#include <algorithm>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UINpcTalk::UINpcTalk() : offset(0), unitrows(0), rowmax(0), scrollable(0), hovered(-1), show_slider(false), draw_text(false), formatted_text(""), formatted_text_pos(0), timestep(0),
		msgtype(0), text_y(0), input(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::DARKGREY, Rectangle<int16_t>(Point<int16_t>(166, 0), Point<int16_t>(460, 20)), 40), input_enabled(false), nummin(0), nummax(0)
	{
		nl::node UtilDlgEx = nl::nx::UI["UIWindow2.img"]["UtilDlgEx"];

		top = UtilDlgEx["t"];
		fill = UtilDlgEx["c"];
		bottom = UtilDlgEx["s"];
		nametag = UtilDlgEx["bar"];

		min_height = TEXT_MIN_HEIGHT + TEXT_MARGIN;

		buttons[Buttons::ALLLEVEL] = std::make_unique<MapleButton>(UtilDlgEx["BtAllLevel"]);
		buttons[Buttons::CLOSE] = std::make_unique<MapleButton>(UtilDlgEx["BtClose"]);
		buttons[Buttons::MYLEVEL] = std::make_unique<MapleButton>(UtilDlgEx["BtMyLevel"]);
		buttons[Buttons::NEXT] = std::make_unique<MapleButton>(UtilDlgEx["BtNext"]);
		buttons[Buttons::NO] = std::make_unique<MapleButton>(UtilDlgEx["BtNo"]);
		buttons[Buttons::OK] = std::make_unique<MapleButton>(UtilDlgEx["BtOK"]);
		buttons[Buttons::PREV] = std::make_unique<MapleButton>(UtilDlgEx["BtPrev"]);
		buttons[Buttons::QAFTER] = std::make_unique<MapleButton>(UtilDlgEx["BtQAfter"]);
		buttons[Buttons::QCNO] = std::make_unique<MapleButton>(UtilDlgEx["BtQCNo"]);
		buttons[Buttons::QCYES] = std::make_unique<MapleButton>(UtilDlgEx["BtQCYes"]);
		buttons[Buttons::QGIVEUP] = std::make_unique<MapleButton>(UtilDlgEx["BtQGiveup"]);
		buttons[Buttons::QNO] = std::make_unique<MapleButton>(UtilDlgEx["BtQNo"]);
		buttons[Buttons::QSTART] = std::make_unique<MapleButton>(UtilDlgEx["BtQStart"]);
		buttons[Buttons::QYES] = std::make_unique<MapleButton>(UtilDlgEx["BtQYes"]);
		buttons[Buttons::YES] = std::make_unique<MapleButton>(UtilDlgEx["BtYes"]);

		name = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE);

		onmoved = [&](bool upwards)
		{
			int16_t shift = upwards ? -unitrows : unitrows;
			bool above = offset + shift >= 0;
			bool below = offset + shift <= rowmax - unitrows;

			if (above && below)
				offset += shift;
		};

		input.set_enter_callback(
			[&](std::string)
			{
				submit_input();
			}
		);

		input.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				close_dialogue();
			}
		);

		UI::get().remove_textfield();
	}

	void UINpcTalk::draw(float inter) const
	{
		Point<int16_t> drawpos = position;
		top.draw(drawpos);
		drawpos.shift_y(top.height());
		fill.draw(DrawArgument(drawpos, Point<int16_t>(0, height)));
		drawpos.shift_y(height);
		bottom.draw(drawpos);
		drawpos.shift_y(bottom.height());

		UIElement::draw(inter);

		int16_t speaker_y = (top.height() + height + bottom.height()) / 2;
		Point<int16_t> speaker_pos = position + Point<int16_t>(22, 11 + speaker_y);
		Point<int16_t> center_pos = speaker_pos + Point<int16_t>(nametag.width() / 2, 0);

		speaker.draw(DrawArgument(center_pos, true));
		nametag.draw(speaker_pos);
		name.draw(center_pos + Point<int16_t>(0, -4));

		int16_t content_y = content_offset();

		if (show_slider)
		{
			// The clip hides the lines which were scrolled over the top of the text
			// area; it has to sit where that area begins, not where the texture
			// behind it does, or the first line loses its tallest glyphs
			int16_t text_min_height = position.y() + text_top();
			text.draw(position + Point<int16_t>(162, content_y), Range<int16_t>(text_min_height, position.y() + top.height() + height), hovered);
			slider.draw(position);
		}
		else
		{
			text.draw(position + Point<int16_t>(166, content_y), hovered);
		}

		if (input_enabled)
			input.draw(Point<int16_t>(0, 0));
	}

	void UINpcTalk::update()
	{
		UIElement::update();

		if (draw_text)
		{
			if (timestep > 4)
			{
				if (formatted_text_pos < formatted_text.size())
				{
					// One unit at a time: a code which is only half written would
					// swallow the text behind it until it arrives
					size_t length = 1;
					textformat::classify(formatted_text, formatted_text_pos, length);

					formatted_text_pos = std::min(formatted_text.size(), formatted_text_pos + length);

					text.change_text(formatted_text.substr(0, formatted_text_pos));
					timestep = 0;
				}
				else
				{
					draw_text = false;
				}
			}
			else
			{
				timestep++;
			}
		}

		if (input_enabled)
			input.update();
	}

	Button::State UINpcTalk::button_pressed(uint16_t buttonid)
	{
		switch (type)
		{
			case TalkType::SENDOK:
			case TalkType::SENDPREV:
			case TalkType::SENDNEXT:
			case TalkType::SENDNEXTPREV:
			{
				// msgType 0: the buttons made active by change_text decide the answer
				switch (buttonid)
				{
					case Buttons::CLOSE:
						close_dialogue();
						break;
					case Buttons::OK:
					case Buttons::NEXT:
						NpcTalkMorePacket(msgtype, 1).dispatch();
						deactivate();
						break;
					case Buttons::PREV:
						NpcTalkMorePacket(msgtype, 0).dispatch();
						deactivate();
						break;
				}

				break;
			}
			case TalkType::SENDYESNO:
			{
				// msgType 1
				switch (buttonid)
				{
					case Buttons::CLOSE:
						close_dialogue();
						break;
					case Buttons::NO:
						NpcTalkMorePacket(msgtype, 0).dispatch();
						deactivate();
						break;
					case Buttons::YES:
						NpcTalkMorePacket(msgtype, 1).dispatch();
						deactivate();
						break;
				}

				break;
			}
			case TalkType::SENDACCEPTDECLINE:
			{
				// msgType 0x0C
				switch (buttonid)
				{
					case Buttons::CLOSE:
						close_dialogue();
						break;
					case Buttons::QNO:
						NpcTalkMorePacket(msgtype, 0).dispatch();
						deactivate();
						break;
					case Buttons::QYES:
						NpcTalkMorePacket(msgtype, 1).dispatch();
						deactivate();
						break;
				}

				break;
			}
			case TalkType::SENDGETTEXT:
			case TalkType::SENDGETNUMBER:
			{
				// The answer is read from the input box
				switch (buttonid)
				{
					case Buttons::CLOSE:
						close_dialogue();
						break;
					case Buttons::OK:
						submit_input();
						break;
				}

				break;
			}
			case TalkType::SENDSIMPLE:
			{
				// The selection itself is sent by the option lines of the text
				if (buttonid == Buttons::CLOSE)
					close_dialogue();

				break;
			}
			default:
			{
				// These modes have no interface of their own yet, so they can only
				// be dismissed
				if (buttonid == Buttons::CLOSE)
					close_dialogue();

				break;
			}
		}

		return Button::State::NORMAL;
	}

	Cursor::State UINpcTalk::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> cursor_relative = cursorpos - position;

		// The entry the cursor is on: its marker turns red and its text is underlined
		if (!draw_text)
			hovered = text.select_at(cursor_relative - Point<int16_t>(show_slider ? 162 : 166, content_offset()));

		if (show_slider && slider.isenabled())
			if (Cursor::State sstate = slider.send_cursor(cursor_relative, clicked))
				return sstate;

		if (input_enabled && input.send_cursor(cursorpos, clicked) == Cursor::State::CLICKING)
			return Cursor::State::CLICKING;

		if (!draw_text)
		{
			// An entry of the dialog is answered with its id, the way the official
			// client answers the row a player clicked on. The server reads it as an
			// int selection (NPCMoreTalkHandler.java:50-53), which
			// NpcTalkMorePacket(int32_t) sends as the answer to a sendSimple dialog.
			int16_t selection = hovered;

			if (selection >= 0)
			{
				if (clicked)
				{
					NpcTalkMorePacket(static_cast<int32_t>(selection)).dispatch();
					deactivate();

					return Cursor::State::CLICKING;
				}

				return Cursor::State::CANCLICK;
			}
		}

		Cursor::State estate = UIElement::send_cursor(clicked, cursorpos);

		if (estate == Cursor::State::CLICKING && clicked && draw_text)
		{
			draw_text = false;
			text.change_text(formatted_text);
		}

		return estate;
	}

	void UINpcTalk::remove_cursor()
	{
		hovered = -1;
	}

	void UINpcTalk::send_scroll(double yoffset)
	{
		// The wheel scrolls the dialog one row at a time, the way the thumb and the
		// arrows of its scrollbar do
		if (show_slider)
			slider.send_scroll(yoffset);
	}

	void UINpcTalk::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			// ESC ends a dialog exactly like its END CHAT button does: close_dialogue
			// picks the response the dialog type has to send. Ending a msgType 0 dialog
			// with 0 only steps the script back (NPCConversationManager's action mode),
			// which leaves the conversation and its manager alive on the server, and
			// every later talk request is dropped because of it.
			close_dialogue();
		}
	}

	UIElement::Type UINpcTalk::get_type() const
	{
		return TYPE;
	}

	// Maps the msgType of the server to the dialog mode it stands for
	// (PacketCreator.getNPCTalk and the getNPCTalk* methods, PacketCreator.java:3333-3419)
	UINpcTalk::TalkType UINpcTalk::get_by_value(int8_t value)
	{
		switch (value)
		{
			case 0x00:
				// Which buttons a msgType 0 dialog has is sent in its text
				return TalkType::SENDOK;
			case 0x01:
				return TalkType::SENDYESNO;
			case 0x02:
				return TalkType::SENDGETTEXT;
			case 0x03:
				return TalkType::SENDGETNUMBER;
			case 0x04:
				return TalkType::SENDSIMPLE;
			case 0x06:
				return TalkType::SENDQUIZ;
			case 0x07:
				return TalkType::SENDSTYLE;
			case 0x0C:
				return TalkType::SENDACCEPTDECLINE;
			case 0x0E:
				return TalkType::SENDMIRROR;
		}

		return TalkType::NONE;
	}

	void UINpcTalk::change_text(const NpcTalkDialogue& dialogue)
	{
		msgtype = dialogue.msgtype;
		type = get_by_value(dialogue.msgtype);

		// A msgType 0 dialog sends the two bytes of its button flags right after the
		// text: sendPrev sends "01 00", sendNext "00 01", sendNextPrev "01 01" and
		// sendOk "00 00" (NPCConversationManager.java:156-171)
		if (type == TalkType::SENDOK)
		{
			if (dialogue.prev && dialogue.next)
				type = TalkType::SENDNEXTPREV;
			else if (dialogue.next)
				type = TalkType::SENDNEXT;
			else if (dialogue.prev)
				type = TalkType::SENDPREV;
		}

		timestep = 0;
		draw_text = true;
		formatted_text_pos = 0;

		// The entries a dialog can be answered with are part of its text and stay
		// there (#L<id>#<label>#l marks them, NPCConversationManager.sendSimple and
		// the npc scripts): FormatText draws and resolves them like the rest, and
		// select_at tells a click on one of them from a click on the body
		formatted_text = dialogue.text;

		text = FormatText(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::DARKGREY, formatted_text, TEXT_WIDTH, TextResolver::get());
		text.set_underline_thickness(Setting<UnderlineThickness>::get().load());

		int16_t text_height = text.height();
		// The descent has to be read before the text is cleared for the reveal: the
		// scroll below sizes the content with the room the last row takes below its
		// baseline, and that room is gone once the layout is empty
		int16_t text_descent = text.descent();

		text.change_text("");

		if (dialogue.speaker == 0)
		{
			std::string strid = std::to_string(dialogue.npcid);
			strid.insert(0, 7 - strid.size(), '0');
			strid.append(".img");

			speaker = nl::nx::Npc[strid]["stand"]["0"];

			std::string namestr = nl::nx::String["Npc.img"][std::to_string(dialogue.npcid)]["name"];
			name.change_text(namestr);
		}
		else
		{
			speaker = Texture();
			name.change_text("");
		}

		// The text sits just below the frame of the window, the way the official client
		// places it, and the window is TEXT_MARGIN taller than the text for it
		text_y = static_cast<int16_t>(top.height() + TEXT_MARGIN);

		int16_t content_height = text_height + text_descent + TEXT_MARGIN;

		height = min_height;
		show_slider = false;
		scrollable = 0;

		if (content_height > height)
		{
			if (content_height > TEXT_MAX_HEIGHT + TEXT_MARGIN)
			{
				height = TEXT_MAX_HEIGHT + TEXT_MARGIN;
				show_slider = true;

				// The rows of the scrollbar are steps of SCROLL_STEP pixels and the last
				// one brings the bottom of the content to the bottom of the view:
				// CUtilDlgEx::SetUtilDlgEx sets the range to (content - view) / 8 + 1.
				// That range rounds the last step down, which leaves up to seven pixels
				// of the last row below the view, so the steps are rounded up here and
				// content_offset stops the last one at scrollable: the bottom of the
				// text lands exactly on the bottom of the view
				// The height of the layout is the baseline of its last row, so the room
				// the glyphs of that row take below its baseline belongs to the content
				// as well: without it the bottom of the last row stays below the view
				int16_t view_height = height - TEXT_MARGIN;
				scrollable = std::max<int16_t>(0, text_height + text_descent - view_height);

				rowmax = (scrollable + SCROLL_STEP - 1) / SCROLL_STEP + 1;
				unitrows = 1;
				offset = 0;

				int16_t slider_y = top.height() - 7;
				slider = Slider(Slider::Type::DEFAULT_SILVER, Range<int16_t>(slider_y, slider_y + height - 20), TEXT_LEFT + TEXT_WIDTH - 2, unitrows, rowmax, onmoved);
			}
			else
			{
				height = content_height;
			}
		}


		for (auto& button : buttons)
		{
			button.second->set_active(false);
			button.second->set_state(Button::State::NORMAL);
		}

		int16_t y_cord = height + 48;

		buttons[Buttons::CLOSE]->set_position(Point<int16_t>(9, y_cord));
		buttons[Buttons::CLOSE]->set_active(true);

		// The mode decides which buttons the dialog can be answered with
		switch (type)
		{
			case TalkType::SENDOK:
			{
				buttons[Buttons::OK]->set_position(Point<int16_t>(471, y_cord));
				buttons[Buttons::OK]->set_active(true);
				break;
			}
			case TalkType::SENDNEXT:
			{
				buttons[Buttons::NEXT]->set_position(Point<int16_t>(471, y_cord));
				buttons[Buttons::NEXT]->set_active(true);
				break;
			}
			case TalkType::SENDPREV:
			{
				buttons[Buttons::PREV]->set_position(Point<int16_t>(471, y_cord));
				buttons[Buttons::PREV]->set_active(true);
				break;
			}
			case TalkType::SENDNEXTPREV:
			{
				// Both buttons share the right end of the button row
				buttons[Buttons::NEXT]->set_position(Point<int16_t>(471, y_cord));
				buttons[Buttons::NEXT]->set_active(true);

				buttons[Buttons::PREV]->set_position(Point<int16_t>(471 - buttons[Buttons::NEXT]->width() - 6, y_cord));
				buttons[Buttons::PREV]->set_active(true);
				break;
			}
			case TalkType::SENDYESNO:
			{
				Point<int16_t> yes_position = Point<int16_t>(389, y_cord);

				buttons[Buttons::YES]->set_position(yes_position);
				buttons[Buttons::YES]->set_active(true);

				buttons[Buttons::NO]->set_position(yes_position + Point<int16_t>(65, 0));
				buttons[Buttons::NO]->set_active(true);
				break;
			}
			case TalkType::SENDACCEPTDECLINE:
			{
				// The accept/decline dialog uses the quest buttons
				Point<int16_t> yes_position = Point<int16_t>(389, y_cord);

				buttons[Buttons::QYES]->set_position(yes_position);
				buttons[Buttons::QYES]->set_active(true);

				buttons[Buttons::QNO]->set_position(yes_position + Point<int16_t>(65, 0));
				buttons[Buttons::QNO]->set_active(true);
				break;
			}
			case TalkType::SENDGETTEXT:
			case TalkType::SENDGETNUMBER:
			{
				// The answer is typed between the close and the ok button
				buttons[Buttons::OK]->set_position(Point<int16_t>(471, y_cord));
				buttons[Buttons::OK]->set_active(true);
				break;
			}
			default:
			{
				break;
			}
		}

		position = Point<int16_t>(400 - top.width() / 2, 240 - height / 2);
		dimension = Point<int16_t>(top.width(), height + 120);

		input_enabled = type == TalkType::SENDGETTEXT || type == TalkType::SENDGETNUMBER;

		if (input_enabled)
		{
			nummin = dialogue.nummin;
			nummax = dialogue.nummax;

			input.set_limit(type == TalkType::SENDGETNUMBER ? 10 : 40);
			input.change_text(type == TalkType::SENDGETNUMBER ? std::to_string(dialogue.numdefault) : dialogue.textdefault);
			input.update(position + Point<int16_t>(166, y_cord), Point<int16_t>(294, 20));
			input.set_state(Textfield::State::FOCUSED);
		}
		else
		{
			input.set_state(Textfield::State::DISABLED);
		}
	}

	int16_t UINpcTalk::text_top() const
	{
		// The text keeps its place inside the window whether it is scrolled or not: the
		// window itself is centered on the screen (see [position]), so a taller text
		// moves the whole window instead of pushing the first lines out of it
		return text_y;
	}

	int16_t UINpcTalk::content_offset() const
	{
		// The rows are steps of SCROLL_STEP pixels, and the last one stops where the
		// bottom of the text reaches the bottom of the view: the range of the
		// scrollbar rounds down to whole rows, so without the clamp the last row of a
		// text would stay half hidden
		return text_top() - std::min<int16_t>(offset * SCROLL_STEP, scrollable);
	}

	void UINpcTalk::submit_input()
	{
		if (type == TalkType::SENDGETNUMBER)
		{
			const std::string& numstr = input.get_text();

			if (numstr.empty() || numstr.find_first_not_of("0123456789") != std::string::npos)
			{
				UI::get().emplace<UIOk>("Only numbers are allowed.", [](bool) {});
				return;
			}

			// More digits than an int can hold are out of range
			int64_t number = numstr.size() > 9 ? -1 : std::stoll(numstr);

			// The bounds come from the encoder (PacketCreator.getNPCTalkNum,
			// PacketCreator.java:3376-3379)
			if (number < 0 || (nummax > nummin && (number < nummin || number > nummax)))
			{
				UI::get().emplace<UIOk>("You may only enter a number between " + std::to_string(nummin) + " and " + std::to_string(nummax) + ".", [](bool) {});
				return;
			}

			// The server reads the entered number as an int selection
			// (NPCMoreTalkHandler.java:50-53)
			NpcTalkMorePacket(msgtype, 1, static_cast<int32_t>(number)).dispatch();
		}
		else if (type == TalkType::SENDGETTEXT)
		{
			// An answer to a getText dialog is the text itself
			// (NPCMoreTalkHandler.java:38-42)
			NpcTalkMorePacket(input.get_text()).dispatch();
		}
		else
		{
			return;
		}

		input.set_state(Textfield::State::DISABLED);
		deactivate();
	}

	void UINpcTalk::close_dialogue()
	{
		// The dialog buttons end a dialog with -1, except for the modes the server
		// continues with a payload of its own: those have to be closed with action 0
		// (NPCMoreTalkHandler.java:34-49)
		int8_t response = -1;

		switch (type)
		{
			case TalkType::SENDGETTEXT:
			case TalkType::SENDGETNUMBER:
			case TalkType::SENDSIMPLE:
			case TalkType::SENDQUIZ:
			case TalkType::SENDSTYLE:
			case TalkType::SENDMIRROR:
				response = 0;
				break;
			default:
				break;
		}

		LOG(LOG_NETWORK, "[UINpcTalk] Closing dialog: msgType=[{}] response=[{}]",
			static_cast<int32_t>(msgtype), static_cast<int32_t>(response));

		NpcTalkMorePacket(msgtype, response).dispatch();

		input.set_state(Textfield::State::DISABLED);
		deactivate();
	}
}