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

#include "Text.h"
#include "TextFormat.h"
#include "Texture.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
	// Analysis of a text before it is laid out, the way the official client does it
	// (GMS v95, CTextAnalyzer::AnalyzeText): the inline format codes are resolved
	// while the text is walked, and what is left is a list of elements - text runs
	// with the font and the colour they are drawn in, the entries of a dialog, and
	// the markers and objects a code stands for. Nothing is measured or positioned
	// here; that is the layout's turn.
	//
	// The caller supplies the game data the codes need - the names (#t, #p, #m, ...)
	// and the canvases of the object codes (#i, #f, #W, #B) - as a Resolver, which
	// keeps this module free of the loading tables.
	namespace textformat
	{
		// The game data the codes of a text are resolved with
		class Resolver
		{
		public:
			virtual ~Resolver() = default;

			virtual std::string item_name(int32_t itemid) const = 0;
			// The number of the item the player's inventory holds, for #c
			virtual int32_t item_count(int32_t itemid) const = 0;
			virtual Texture item_icon(int32_t itemid) const = 0;
			virtual Texture skill_icon(int32_t skillid) const = 0;
			// The canvas behind a path of the client's own data, for #f, #W and #B
			virtual Texture ui_image(const std::string& path) const = 0;
			virtual std::string mob_name(int32_t mobid) const = 0;
			virtual std::string npc_name(int32_t npcid) const = 0;
			virtual std::string map_name(int32_t mapid) const = 0;
			virtual std::string skill_name(int32_t skillid) const = 0;
			virtual std::string quest_name(int32_t questid) const = 0;
			virtual std::string player_name() const = 0;
		};

		// One piece of an analyzed text, in the order the official analyzer produces
		// its elements. The codes which only switch the state of a text (#k, #e, #l,
		// ...) produce no element of their own.
		struct Element
		{
			enum class Kind
			{
				TEXT,		// Drawn with [font] and [color]
				OBJECT,		// Drawn as [icon], followed by [text] when it has one
				MARKER,		// #E #I #S #K #w and ##: the official client draws a marker
				SELECT		// #L<n>#: the entry [id] a dialog offers starts here
			};

			Kind kind = Kind::TEXT;
			Text::Font font = Text::Font::A11M;
			Color::Name color = Color::Name::BLACK;
			// The text of the element: a resolved name for the codes which carry one,
			// the characters themselves for plain text and for a '#' no code follows
			std::string text;
			// The canvas of an object element, empty when the data has none
			Texture icon;
			// The code the element came from, 0 for plain text
			char code = 0;
			// The payload of that code, which identifies the item, NPC, map and so on
			int32_t id = 0;
			// The line the element belongs to, counted over the explicit line breaks
			int16_t line = 0;
			// The entry of a #L range the element belongs to, -1 outside of one
			int16_t select = -1;
			// Whether the line ends behind this element
			bool line_break = false;
			// The bytes of the source text the element was made from
			size_t first = 0;
			size_t last = 0;
		};

		struct AnalyzedText
		{
			std::vector<Element> elements;
			// The lines the elements are on, the trailing empty ones not counted
			int16_t lines = 0;
		};

		// Walks [text] once and resolves its format codes through [resolver]; [font]
		// and [color] are the state the text starts with. The [mode] picks the dialect
		// the text is written in (see Mode): a dialog, which is what the server and
		// the quest texts send, or a description, which is what the tooltips draw.
		AnalyzedText analyze(const std::string& text, Text::Font font, Color::Name color, const Resolver& resolver, Mode mode = Mode::DIALOG);
	}
}
