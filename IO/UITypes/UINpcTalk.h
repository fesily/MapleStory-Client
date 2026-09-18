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

#include "../UIElement.h"

#include "../Components/Slider.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Text.h"

#include <vector>

namespace ms
{
	// Everything the server sends with an NPC_TALK packet
	// See PacketCreator.getNPCTalk and its siblings (PacketCreator.java:3333-3419)
	struct NpcTalkDialogue
	{
		int32_t npcid = 0;
		// Raw msgType, echoed back to the server in NPC_TALK_MORE
		// (NPCMoreTalkHandler.java:34-46)
		int8_t msgtype = 0;
		int8_t speaker = 0;
		// Button flags of the msgType 0 dialog: sendPrev sends "01 00", sendNext
		// "00 01", sendNextPrev "01 01" and sendOk "00 00"
		// (NPCConversationManager.java:156-171)
		int8_t prev = 0;
		int8_t next = 0;
		std::string text;
		// msgType 2: the text the input box starts with
		std::string textdefault;
		// msgType 3: default, minimum and maximum of the number input
		int32_t numdefault = 0;
		int32_t nummin = 0;
		int32_t nummax = 0;
	};

	class UINpcTalk : public UIElement
	{
	public:
		// Dialog modes. The msgType values are only shared with mode 0 and 1, so the
		// wire value is translated instead of cast (PacketCreator.getNPCTalk and the
		// getNPCTalk* methods below it)
		enum TalkType : int8_t
		{
			NONE = -1,
			SENDOK,				// msgType 0, no button flags
			SENDPREV,			// msgType 0, prev flag
			SENDNEXT,			// msgType 0, next flag
			SENDNEXTPREV,		// msgType 0, prev and next flags
			SENDYESNO,			// msgType 1 - sendYesNo
			SENDGETTEXT,		// msgType 2 - getNPCTalkText
			SENDGETNUMBER,		// msgType 3 - getNPCTalkNum
			SENDSIMPLE,			// msgType 4 - sendSimple
			SENDQUIZ,			// msgType 6 - OnAskQuiz
			SENDSTYLE,			// msgType 7 - getNPCTalkStyle
			SENDACCEPTDECLINE,	// msgType 0x0C - sendAcceptDecline
			SENDMIRROR,			// msgType 0x0E - getDimensionalMirror
			LENGTH
		};

		static constexpr Type TYPE = UIElement::Type::NPCTALK;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UINpcTalk();

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		void change_text(const NpcTalkDialogue& dialogue);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		TalkType get_by_value(int8_t value);
		std::string format_text(const std::string& tx, const int32_t& npcid);
		// Splits the selectable entries (#L<id>#label#l) out of a sendSimple text
		std::string parse_options(const std::string& tx);
		int8_t option_at(Point<int16_t> cursorpos) const;
		int16_t content_offset() const;
		void submit_input();
		void close_dialogue();

		static constexpr int16_t MAX_HEIGHT = 248;

		enum Buttons
		{
			ALLLEVEL,
			CLOSE,
			MYLEVEL,
			NEXT,
			NO,
			OK,
			PREV,
			QAFTER,
			QCNO,
			QCYES,
			QGIVEUP,
			QNO,
			QSTART,
			QYES,
			YES
		};

		// A selectable entry of a sendSimple dialog
		struct Option
		{
			int32_t id;
			int16_t y;
			Text label;
		};

		Texture top;
		Texture fill;
		Texture bottom;
		Texture nametag;
		Texture speaker;

		Text text;
		Text name;

		int16_t height;
		int16_t offset;
		int16_t unitrows;
		int16_t rowmax;
		int16_t min_height;

		bool show_slider;
		bool draw_text;
		Slider slider;
		TalkType type;
		std::string formatted_text;
		size_t formatted_text_pos;
		uint16_t timestep;

		int8_t msgtype;
		int16_t text_y;
		std::vector<Option> options;
		int8_t hovered_option;
		Textfield input;
		bool input_enabled;
		int32_t nummin;
		int32_t nummax;

		std::function<void(bool)> onmoved;
	};
}