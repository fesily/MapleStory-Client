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
#include "TextAnalyzer.h"

#include <vector>

namespace ms
{
	// A text of the game's own format: it is analyzed (every code resolved through a
	// Resolver) and laid out, so that the names, dialog entries, icons and inline
	// images of a text the server or the data files send can be drawn. Text stays the
	// fast path for the plain labels the client builds itself.
	//
	// The layout follows the official client (CTextAnalyzer::AnalyzeText): an object
	// is placed as one piece and moves to the next line when it does not fit, the
	// text of an element is wrapped at the spaces a word breaks at, and a line is as
	// tall as its tallest element, with everything sitting on the bottom of its line.
	class FormatText
	{
	public:
		FormatText();
		FormatText(Text::Font font, Text::Alignment alignment, Color::Name color, const std::string& text, uint16_t maxwidth, const textformat::Resolver& resolver, textformat::Mode mode = textformat::Mode::DIALOG);

		void change_text(const std::string& text);
		void change_color(Color::Name color);
		void draw(const DrawArgument& args, int16_t selected = -1) const;
		// The entries a click would answer are drawn with this: the marker of the one
		// under the cursor turns red and its text is underlined
		void draw(const DrawArgument& args, const Range<int16_t>& vertical, int16_t selected = -1) const;

		// The entry of a dialog under a point of the text, -1 outside of one: the
		// entries stay where the text puts them (#L<n># marks the row) and are
		// answered by their id, the way the official client does it
		int16_t select_at(Point<int16_t> cursorpos) const;

		// How thick the line under the entry the cursor is on is drawn, in pixels; 0
		// leaves it out
		void set_underline_thickness(int16_t thickness);

		const textformat::AnalyzedText& analyzed() const;
		const std::string& get_text() const;
		size_t length() const;
		int16_t width() const;
		int16_t height() const;
		// How far the lowest glyph of the text reaches below its last baseline. The
		// height of a layout is the baseline of its last row, so a window which
		// scrolls the text needs this to know where its bottom really is
		int16_t descent() const;
		Point<int16_t> dimensions() const;

	private:
		// A canvas of the analyzed text where the layout put it
		struct Placed
		{
			size_t element;
			int16_t x;
			int16_t y;
		};

		// The area an entry of a dialog takes, so that a click can be told apart
		struct Entry
		{
			int16_t select;
			int16_t x;
			int16_t y;
			int16_t width;
			int16_t height;
			Color::Name color;
			// Whether this is where the entry starts: only there does its marker and
			// its underline belong
			bool starts;
		};

		void relayout();
		int16_t measure(Text::Font font, const std::string& str, size_t first, size_t last) const;

		// The lowest pixels the glyphs of the last row of text put below its baseline
		int16_t measured_descent() const;

		Text::Font font;
		Text::Alignment alignment;
		Color::Name color;
		uint16_t maxwidth;
		textformat::Mode mode;
		const textformat::Resolver* resolver;

		std::string text;
		textformat::AnalyzedText parsed;
		// The texts of the elements, which the runs of the layout index
		std::string cooked;
		Text::Layout layout;
		std::vector<Placed> placed;
		std::vector<Entry> entries;
		int16_t descenders;

		// The assets of the entries of a dialog, loaded when the text has one, and how
		// thick the underline is drawn (see UnderlineThickness)
		Texture marker;
		Texture marker_on;
		Texture underline;
		int16_t underline_height;
	};
}
