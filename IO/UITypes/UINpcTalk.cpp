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
	UINpcTalk::UINpcTalk() : offset(0), unitrows(0), rowmax(0), show_slider(false), draw_text(false), formatted_text(""), formatted_text_pos(0), timestep(0),
		msgtype(0), text_y(0), hovered_option(-1), input(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::DARKGREY, Rectangle<int16_t>(Point<int16_t>(166, 0), Point<int16_t>(460, 20)), 40), input_enabled(false), nummin(0), nummax(0)
	{
		nl::node UtilDlgEx = nl::nx::UI["UIWindow2.img"]["UtilDlgEx"];

		top = UtilDlgEx["t"];
		fill = UtilDlgEx["c"];
		bottom = UtilDlgEx["s"];
		nametag = UtilDlgEx["bar"];

		min_height = 8 * fill.height() + 14;

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
			int16_t text_min_height = position.y() + top.height() - 1;
			text.draw(position + Point<int16_t>(162, content_y), Range<int16_t>(text_min_height, text_min_height + height - 18));
			slider.draw(position);
		}
		else
		{
			text.draw(position + Point<int16_t>(166, content_y));
		}

		// The entries of a sendSimple dialog are listed below the text and are clickable
		if (!draw_text)
		{
			for (const Option& option : options)
			{
				Point<int16_t> option_pos = position + Point<int16_t>(166, content_y + option.y);

				if (show_slider)
				{
					int16_t text_min_height = position.y() + top.height() - 1;
					option.label.draw(DrawArgument(option_pos), Range<int16_t>(text_min_height, text_min_height + height - 18));
				}
				else
				{
					option.label.draw(option_pos);
				}
			}
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
					std::string t = text.get_text();
					char c = formatted_text[formatted_text_pos];

					text.change_text(t + c);

					formatted_text_pos++;
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

		if (show_slider && slider.isenabled())
			if (Cursor::State sstate = slider.send_cursor(cursor_relative, clicked))
				return sstate;

		if (input_enabled && input.send_cursor(cursorpos, clicked) == Cursor::State::CLICKING)
			return Cursor::State::CLICKING;

		if (!draw_text && !options.empty())
		{
			int8_t option = option_at(cursor_relative);

			if (option != hovered_option)
			{
				if (hovered_option >= 0)
					options[static_cast<size_t>(hovered_option)].label.change_color(Color::Name::MEDIUMBLUE);

				if (option >= 0)
					options[static_cast<size_t>(option)].label.change_color(Color::Name::BLUE);
			}

			hovered_option = option;

			if (option >= 0)
			{
				if (clicked)
				{
					// The server reads the chosen entry as an int selection
					// (NPCMoreTalkHandler.java:50-53), which NpcTalkMorePacket(int32_t)
					// sends as the answer to a sendSimple dialog
					NpcTalkMorePacket(options[static_cast<size_t>(option)].id).dispatch();
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

	// TODO: Move this to GraphicsGL?
	std::string UINpcTalk::format_text(const std::string& tx, const int32_t& npcid)
	{
		// The closing '#' belongs to the code, so it is replaced with it: leaving it
		// behind puts a stray '#' the layout draws into the text
		std::string formatted_text = tx;
		size_t begin = formatted_text.find("#p");

		if (begin != std::string::npos)
		{
			size_t end = formatted_text.find("#", begin + 1);

			if (end != std::string::npos)
			{
				std::string namestr = nl::nx::String["Npc.img"][std::to_string(npcid)]["name"];
				formatted_text.replace(begin, end - begin + 1, namestr);
			}
		}

		begin = formatted_text.find("#h");

		if (begin != std::string::npos)
		{
			size_t end = formatted_text.find("#", begin + 1);

			if (end != std::string::npos)
			{
				std::string charstr = Stage::get().get_player().get_name();
				formatted_text.replace(begin, end - begin + 1, charstr);
			}
		}

		begin = formatted_text.find("#t");

		if (begin != std::string::npos)
		{
			size_t end = formatted_text.find("#", begin + 1);

			if (end != std::string::npos)
			{
				size_t b = begin + 2;
				int32_t itemid = std::stoi(formatted_text.substr(b, end - b));
				std::string itemname = nl::nx::String["Consume.img"][itemid]["name"];

				formatted_text.replace(begin, end - begin + 1, itemname);
			}
		}

		return formatted_text;
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
		hovered_option = -1;
		options.clear();

		std::string bodytext = dialogue.text;

		// The entries a sendSimple dialog can be answered with are part of its text
		// (NPCConversationManager.sendSimple, NPCConversationManager.java:188-191)
		if (type == TalkType::SENDSIMPLE)
			bodytext = parse_options(dialogue.text);

		formatted_text = format_text(bodytext, dialogue.npcid);

		text = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::DARKGREY, formatted_text, 320);

		int16_t text_height = text.height();

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

		// The entries are listed below the text, so they add to the dialog height
		int16_t content_height = text_height;

		for (Option& option : options)
		{
			option.y = content_height;
			content_height += option.label.height();
		}

		height = min_height;
		show_slider = false;

		if (content_height > height)
		{
			if (content_height > MAX_HEIGHT)
			{
				height = MAX_HEIGHT;
				show_slider = true;
				rowmax = content_height / 400 + 1;
				unitrows = 1;

				int16_t slider_y = top.height() - 7;
				slider = Slider(Slider::Type::DEFAULT_SILVER, Range<int16_t>(slider_y, slider_y + height - 20), top.width() - 26, unitrows, rowmax, onmoved);
			}
			else
			{
				height = content_height;
			}
		}

		text_y = 48 - (height - min_height);

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

	// The selectable entries of a sendSimple text are marked as #L<id>#<label>#l
	// (NPCConversationManager.sendSimple and the npc scripts); they are listed below
	// the remaining text and answered with their id
	std::string UINpcTalk::parse_options(const std::string& tx)
	{
		std::string bodytext;
		size_t pos = 0;

		while (pos < tx.size())
		{
			size_t begin = tx.find("#L", pos);

			if (begin == std::string::npos)
			{
				bodytext.append(tx, pos, std::string::npos);
				break;
			}

			bodytext.append(tx, pos, begin - pos);

			size_t idend = tx.find('#', begin + 2);

			if (idend == std::string::npos)
			{
				bodytext.append(tx, begin, std::string::npos);
				break;
			}

			std::string idstr = tx.substr(begin + 2, idend - begin - 2);

			if (idstr.empty() || idstr.find_first_not_of("0123456789") != std::string::npos)
			{
				// Not an entry after all
				bodytext.append(tx, begin, idend + 1 - begin);
				pos = idend + 1;
				continue;
			}

			// Some texts do not close an entry with #l, in which case it ends at the next
			// entry or at the end of its line
			size_t labelend = std::min(tx.find("#l", idend + 1), tx.find("#L", idend + 1));
			size_t lineend = tx.find("\n", idend + 1);

			if (lineend < labelend)
			{
				// The line break itself stays out of the entry
				labelend = lineend;
				pos = lineend + 1;

				if (labelend > idend + 1 && tx[labelend - 1] == '\r')
					labelend--;
			}
			else if (labelend != std::string::npos)
			{
				pos = labelend + 2;
			}
			else
			{
				labelend = tx.size();
				pos = tx.size();
			}

			Option option;
			option.id = std::stoi(idstr);
			option.y = 0;
			option.label = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::MEDIUMBLUE, tx.substr(idend + 1, labelend - idend - 1), 320);

			options.push_back(option);
		}

		return bodytext;
	}

	int8_t UINpcTalk::option_at(Point<int16_t> cursorpos) const
	{
		int16_t content_y = content_offset();

		for (size_t i = 0; i < options.size(); i++)
		{
			Point<int16_t> left_top = Point<int16_t>(166, content_y + options[i].y);
			Point<int16_t> right_bottom = left_top + Point<int16_t>(options[i].label.width(), options[i].label.height());

			if (show_slider)
			{
				// Entries scrolled out of the window cannot be clicked
				int16_t text_min_height = top.height() - 1;

				if (left_top.y() < text_min_height || right_bottom.y() > text_min_height + height - 18)
					continue;
			}

			Rectangle<int16_t> bounds(left_top, right_bottom);

			if (bounds.contains(cursorpos))
				return static_cast<int8_t>(i);
		}

		return -1;
	}

	int16_t UINpcTalk::content_offset() const
	{
		// A dialog whose content does not fit is scrolled instead of moved down
		return show_slider ? 19 - offset * 400 : text_y;
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

		LOG(LOG_NETWORK, "[UINpcTalk] Closing dialog: msgType=[" << static_cast<int32_t>(msgtype)
			<< "] response=[" << static_cast<int32_t>(response) << "]");

		NpcTalkMorePacket(msgtype, response).dispatch();

		input.set_state(Textfield::State::DISABLED);
		deactivate();
	}
}