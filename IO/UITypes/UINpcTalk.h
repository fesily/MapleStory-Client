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

#include "../../Graphics/FormatText.h"
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
		void send_scroll(double yoffset) override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

		void change_text(const NpcTalkDialogue& dialogue);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		TalkType get_by_value(int8_t value);
		// Where the text of the dialog starts, which is also where its area is clipped
		int16_t text_top() const;
		// The y of the text inside the window, moved by the rows the dialog is
		// scrolled by
		int16_t content_offset() const;
		void submit_input();
		void close_dialogue();

		// The text area of a dialog, taken from the official client: for the dialog an
		// NPC talks through, CUtilDlgEx::GetBasicCTWidth (0x57ADD0) returns 0x155 and
		// GetCTHeight_Min and GetCTHeight_Max (0x57AE90, 0x57AE40) return 0x6E and
		// 0xF0. The window itself is 519 pixels wide, which is the width of the
		// UtilDlgEx textures of UIWindow2.img
		static constexpr int16_t TEXT_WIDTH = 341;
		static constexpr int16_t TEXT_MIN_HEIGHT = 110;
		static constexpr int16_t TEXT_MAX_HEIGHT = 240;
		// Where the text starts: the speaker column takes this much of the window, so
		// the text ends where its scrollbar begins
		static constexpr int16_t TEXT_LEFT = 166;
		// How far below the top frame of the window the text starts
		static constexpr int16_t TEXT_MARGIN = 4;
		// How far one row of the scrollbar moves the text. The official dialog keeps
		// its scroll in pixels and the scrollbar reports a row which
		// CUtilDlgEx::OnChildNotify turns into row * 8 (0x57AFB0).
		static constexpr int16_t SCROLL_STEP = 8;

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

		Texture top;
		Texture fill;
		Texture bottom;
		Texture nametag;
		Texture speaker;

		FormatText text;
		Text name;

		int16_t height;
		int16_t offset;
		int16_t unitrows;
		int16_t rowmax;
		// The entry of the dialog the cursor is on, -1 outside of one
		int16_t hovered;
		// How far the text can be scrolled at most, in pixels
		int16_t scrollable;
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
		Textfield input;
		bool input_enabled;
		int32_t nummin;
		int32_t nummax;

		std::function<void(bool)> onmoved;
	};
}