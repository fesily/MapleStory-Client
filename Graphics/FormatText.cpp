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
#include "FormatText.h"

#include "GraphicsGL.h"

#include <algorithm>
#include <cstring>
#include <tuple>

namespace
{
	// The resolver of a text whose caller has none: every lookup stays empty, so the
	// codes which need data are dropped like an unresolved one
	struct EmptyResolver : ms::textformat::Resolver
	{
		std::string item_name(int32_t) const override { return std::string(); }
		int32_t item_count(int32_t) const override { return 0; }
		ms::Texture item_icon(int32_t) const override { return ms::Texture(); }
		ms::Texture skill_icon(int32_t) const override { return ms::Texture(); }
		ms::Texture ui_image(const std::string&) const override { return ms::Texture(); }
		std::string mob_name(int32_t) const override { return std::string(); }
		std::string npc_name(int32_t) const override { return std::string(); }
		std::string map_name(int32_t) const override { return std::string(); }
		std::string skill_name(int32_t) const override { return std::string(); }
		std::string quest_name(int32_t) const override { return std::string(); }
		std::string player_name() const override { return std::string(); }
	};

	const EmptyResolver EMPTY_RESOLVER;

	// The width the marker of a dialog entry takes, the official analyzer gives an
	// #L element a fixed width of 18
	constexpr int16_t SELECT_WIDTH = 18;
}

namespace ms
{
	FormatText::FormatText() : FormatText(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, "", 0, EMPTY_RESOLVER) {}

	FormatText::FormatText(Text::Font f, Text::Alignment a, Color::Name c, const std::string& t, uint16_t mw, const textformat::Resolver& r, textformat::Mode m) : font(f), alignment(a), color(c), maxwidth(mw == 0 ? 800 : mw), mode(m), resolver(&r), descenders(0), underline_height(1)
	{
		change_text(t);
	}

	void FormatText::change_text(const std::string& t)
	{
		text = t;

		relayout();
		descenders = measured_descent();
	}

	void FormatText::change_color(Color::Name c)
	{
		color = c;

		relayout();
		descenders = measured_descent();
	}

	// The width one character of a text takes: a tab is worth four spaces, the way
	// the layout builder has always placed them
	static int16_t char_width(GraphicsGL& graphics, Text::Font font, uint32_t codepoint)
	{
		if (codepoint == '\t')
			return graphics.getchar(font, ' ').ax * 4;

		return graphics.getchar(font, codepoint).ax;
	}

	int16_t FormatText::measure(Text::Font mfont, const std::string& str, size_t first, size_t last) const
	{
		GraphicsGL& graphics = GraphicsGL::get();

		int16_t width = 0;

		for (size_t pos = first; pos < last;)
		{
			size_t length = 0;
			uint32_t codepoint = textformat::utf8_decode(str, pos, length);

			width += char_width(graphics, mfont, codepoint);

			pos += length;
		}

		return width;
	}

	void FormatText::relayout()
	{
		parsed = textformat::analyze(text, font, color, *resolver, mode);

		if (parsed.elements.empty())
		{
			cooked.clear();
			layout = Text::Layout();
			placed.clear();
			entries.clear();

			return;
		}

		GraphicsGL& graphics = GraphicsGL::get();

		// The runs of the layout index one string which holds the text of the elements
		cooked.clear();

		std::vector<size_t> offsets;
		offsets.reserve(parsed.elements.size());

		for (const textformat::Element& element : parsed.elements)
		{
			offsets.push_back(cooked.size());
			cooked += element.text;
		}

		const int16_t linespace = graphics.linespace(font);

		std::vector<Text::Layout::Line> lines;
		std::vector<Text::Layout::Word> words;
		std::vector<int16_t> advances(cooked.size() + 1, 0);
		std::vector<Placed> objects;
		std::vector<std::pair<size_t, int16_t>> canvases;
		// The runs of the line being built, with the element and the width each takes
		std::vector<std::tuple<size_t, int16_t, int16_t, size_t>> runs;

		int16_t cursor = 0;
		int16_t current = 0;
		int16_t linewidth = 0;
		int16_t lineheight = linespace;
		int16_t line_top = 0;
		int16_t largest = 0;

		// Closes the line being built: everything sits on its bottom, which is the
		// baseline the text is drawn from
		auto close_line = [&]()
		{
			int16_t baseline = static_cast<int16_t>(line_top + lineheight);
			int16_t origin = 0;

			switch (alignment)
			{
				case Text::Alignment::CENTER:
					origin = static_cast<int16_t>(-linewidth / 2);
					break;
				case Text::Alignment::RIGHT:
					origin = -linewidth;
					break;
				default:
					break;
			}

			for (const std::tuple<size_t, int16_t, int16_t, size_t>& run : runs)
			{
				size_t index = std::get<0>(run);
				const textformat::Element& element = parsed.elements[index];

				if (element.select < 0)
					continue;

				// The marker of an entry sits in front of the #L marker of its text,
				// so the area of the entry starts there, and only the run which starts
				// the text carries the marker and the underline
				bool starts = std::get<3>(run) == 0 && index > 0 && parsed.elements[index - 1].kind == textformat::Element::Kind::SELECT;

				entries.push_back({ element.select, static_cast<int16_t>(std::get<1>(run) + origin - SELECT_WIDTH), static_cast<int16_t>(baseline - lineheight), static_cast<int16_t>(std::get<2>(run) + SELECT_WIDTH), lineheight, element.color, starts });
			}

			for (const std::pair<size_t, int16_t>& canvas : canvases)
			{
				const textformat::Element& element = parsed.elements[canvas.first];
				int16_t canvasheight = element.icon.is_valid() ? element.icon.height() : 0;

				objects.push_back({ canvas.first, static_cast<int16_t>(canvas.second + origin), static_cast<int16_t>(baseline - canvasheight) });
			}

			lines.push_back({ std::move(words), { origin, baseline } });
			words.clear();
			canvases.clear();
			runs.clear();

			largest = std::max(largest, linewidth);
			line_top = baseline;
			lineheight = linespace;
			linewidth = 0;
			cursor = 0;
		};

		for (size_t i = 0; i < parsed.elements.size(); i++)
		{
			const textformat::Element& element = parsed.elements[i];
			const size_t base = offsets[i];

			// The line an element belongs to comes from the analysis, which counts every
			// break: closing the lines in between keeps the blank lines a text asks for
			// blank, the way the official analyzer's nLine does. It has to happen before
			// the element is placed, or the first element behind a break lands on the
			// line the break ended
			while (current < element.line)
			{
				close_line();
				current++;
			}

			if (element.kind == textformat::Element::Kind::OBJECT && element.icon.is_valid())
			{
				int16_t canvaswidth = element.icon.width();
				int16_t canvasheight = element.icon.height();

				// An object is atomic: it moves to the next line instead of being cut
				if (cursor > 0 && cursor + canvaswidth > maxwidth)
					close_line();

				canvases.emplace_back(i, cursor);
				cursor += canvaswidth;
				linewidth = std::max(linewidth, cursor);
				lineheight = std::max<int16_t>(lineheight, canvasheight);
			}
			else if (element.kind == textformat::Element::Kind::SELECT)
			{
				// The 18 pixels an #L element takes: its marker is drawn with the
				// entry, so that the one the cursor is on can be the red one
				cursor += SELECT_WIDTH;
				linewidth = std::max(linewidth, cursor);
			}

			// The text of the element (a name in front of a canvas, or the characters)
			for (size_t pos = 0; pos < element.text.size();)
			{
				// A word and the spaces behind it stay together, so a break keeps the
				// space at the end of the line it belongs to
				size_t wordend = element.text.find(' ', pos);

				if (wordend == std::string::npos)
					wordend = element.text.size();

				// A description may also break behind one of ' . , ; :', which is what
				// the tooltip renderer of the official client treats as a break point
				if (mode == textformat::Mode::DESCRIPTION)
				{
					for (size_t at = pos; at < wordend; at++)
					{
						if (std::strchr(".,;:", element.text[at]))
						{
							wordend = at + 1;
							break;
						}
					}
				}

				size_t chunkend = wordend;

				while (chunkend < element.text.size() && element.text[chunkend] == ' ')
					chunkend++;

				int16_t wordwidth = measure(element.font, element.text, pos, wordend);

				if (cursor > 0 && cursor + wordwidth > maxwidth)
					close_line();

				if (wordwidth > maxwidth)
				{
					// A word longer than a whole line is filled in, character by character
					while (pos < wordend)
					{
						size_t length = 0;
						uint32_t codepoint = textformat::utf8_decode(element.text, pos, length);
						int16_t charwidth = char_width(graphics, element.font, codepoint);

						if (cursor > 0 && cursor + charwidth > maxwidth)
							close_line();

						advances[base + pos] = cursor;
						words.push_back({ base + pos, base + pos + length, element.font, element.color });
						cursor += charwidth;
						linewidth = std::max(linewidth, cursor);

						pos += length;
					}

					continue;
				}

				int16_t runwidth = measure(element.font, element.text, pos, chunkend);

				advances[base + pos] = cursor;
				words.push_back({ base + pos, base + chunkend, element.font, element.color });
				runs.emplace_back(i, cursor, runwidth, pos);
				cursor += runwidth;
				linewidth = std::max(linewidth, cursor);

				pos = chunkend;
			}

		}

		close_line();

		layout = Text::Layout(lines, advances, largest, line_top, cursor, line_top);
		placed = std::move(objects);

		// A text without entries never draws them, so its markers are not loaded at all
		if (!entries.empty())
		{
			if (!marker.is_valid())
			{
				marker = resolver->ui_image("UI/UIWindow2.img/UtilDlgEx/dot0");
				marker_on = resolver->ui_image("UI/UIWindow2.img/UtilDlgEx/dot1");
				underline = resolver->ui_image("UI/UIWindow2.img/UtilDlgEx/line");
			}

			// The thickness is the caller's to set (see set_underline_thickness): it
			// comes from the settings, and 0 leaves the line out
		}
	}

	void FormatText::draw(const DrawArgument& args, int16_t selected) const
	{
		draw(args, Range<int16_t>(), selected);
	}

	void FormatText::draw(const DrawArgument& args, const Range<int16_t>& vertical, int16_t selected) const
	{
		GraphicsGL& graphics = GraphicsGL::get();

		graphics.drawtext(args, vertical, cooked, layout, font, color, Text::Background::NONE);

		// The range the caller passes is the visible band in the coordinates of the
		// screen, while the rectangles of the layout are relative to the text: the
		// position the text is drawn at puts the two together
		GLshort text_y = static_cast<GLshort>(args.getpos().y());
		GLshort view_top = vertical.first();
		GLshort view_bottom = vertical.second();

		for (const Placed& canvas : placed)
		{
			const textformat::Element& element = parsed.elements[canvas.element];

			if (!element.icon.is_valid())
				continue;

			int16_t top = static_cast<int16_t>(text_y + canvas.y);
			int16_t bottom = static_cast<int16_t>(top + element.icon.height());
			int16_t cut_top = view_top > 0 ? std::max<int16_t>(0, static_cast<int16_t>(view_top - top)) : 0;
			int16_t cut_bottom = view_bottom > 0 ? std::max<int16_t>(0, static_cast<int16_t>(bottom - view_bottom)) : 0;

			if (cut_top + cut_bottom < element.icon.height())
				element.icon.draw(args + DrawArgument(Point<int16_t>(canvas.x, canvas.y)), Range<int16_t>(cut_top, cut_bottom));
		}

		// The entries of a dialog: every one keeps its marker, the one under the cursor
		// gets the red one and a line of its own colour under its text
		for (const Entry& entry : entries)
		{
			if (!entry.starts)
				continue;

			// Every rectangle is clipped against the band on its own: a cut larger
			// than the texture would sample whatever sits beside it in the atlas
			int16_t entry_bottom = static_cast<int16_t>(text_y + entry.y + entry.height);
			int16_t entry_top = static_cast<int16_t>(entry_bottom - entry.height);
			int16_t cut_top = view_top > 0 ? std::max<int16_t>(0, static_cast<int16_t>(view_top - entry_top)) : 0;
			int16_t cut_bottom = view_bottom > 0 ? std::max<int16_t>(0, static_cast<int16_t>(entry_bottom - view_bottom)) : 0;

			if (cut_top + cut_bottom >= entry.height)
				continue;

			bool on = entry.select == selected;

			if (on && underline.is_valid() && underline_height > 0)
			{
				int16_t line_top = static_cast<int16_t>(entry.y + entry.height + 1);
				int16_t lcut_top = view_top > 0 ? std::max<int16_t>(0, static_cast<int16_t>(view_top - (text_y + line_top))) : 0;
				int16_t lcut_bottom = view_bottom > 0 ? std::max<int16_t>(0, static_cast<int16_t>(text_y + line_top + underline_height - view_bottom)) : 0;

				if (lcut_top + lcut_bottom < underline_height)
				{
					const GLfloat* rgb = Color::colors[entry.color];
					Color linecolor(rgb[0], rgb[1], rgb[2], 1.0f);
					Point<int16_t> linepos(static_cast<int16_t>(entry.x + SELECT_WIDTH), line_top);
					DrawArgument lineargs = DrawArgument(linepos, linepos, Point<int16_t>(entry.width - SELECT_WIDTH, underline_height), 1.0f, 1.0f, linecolor, 0.0f);

					underline.draw(args + lineargs, Range<int16_t>(lcut_top, lcut_bottom));
				}
			}

			const Texture& entrymarker = on ? marker_on : marker;

			if (entrymarker.is_valid())
			{
				int16_t marker_y = static_cast<int16_t>(entry.y + entry.height - entrymarker.height());
				int16_t mcut_top = view_top > 0 ? std::max<int16_t>(0, static_cast<int16_t>(view_top - (text_y + marker_y))) : 0;
				int16_t mcut_bottom = view_bottom > 0 ? std::max<int16_t>(0, static_cast<int16_t>(text_y + marker_y + entrymarker.height() - view_bottom)) : 0;

				if (mcut_top + mcut_bottom < entrymarker.height())
					entrymarker.draw(args + DrawArgument(Point<int16_t>(entry.x, marker_y)), Range<int16_t>(mcut_top, mcut_bottom));
			}
		}
	}

	int16_t FormatText::select_at(Point<int16_t> cursorpos) const
	{
		for (const Entry& entry : entries)
		{
			if (cursorpos.x() >= entry.x && cursorpos.x() < entry.x + entry.width
				&& cursorpos.y() >= entry.y && cursorpos.y() < entry.y + entry.height)
				return entry.select;
		}

		return -1;
	}

	const textformat::AnalyzedText& FormatText::analyzed() const
	{
		return parsed;
	}

	const std::string& FormatText::get_text() const
	{
		return text;
	}

	size_t FormatText::length() const
	{
		return text.size();
	}

	int16_t FormatText::width() const
	{
		return layout.width();
	}

	int16_t FormatText::height() const
	{
		return layout.height();
	}

	void FormatText::set_underline_thickness(int16_t thickness)
	{
		underline_height = thickness;
	}

	int16_t FormatText::descent() const
	{
		return descenders;
	}

	int16_t FormatText::measured_descent() const
	{
		// The glyphs of the last row of text decide how far the text reaches below
		// its baseline: the rows behind them have no ink of their own
		const textformat::Element* last = nullptr;

		for (const textformat::Element& element : parsed.elements)
		{
			if (!element.text.empty())
				last = &element;
		}

		if (last == nullptr)
			return 0;

		GraphicsGL& graphics = GraphicsGL::get();
		int16_t measured = 0;

		for (size_t pos = 0; pos < last->text.size();)
		{
			size_t length = 0;
			uint32_t codepoint = textformat::utf8_decode(last->text, pos, length);
			const auto& ch = graphics.getchar(last->font, codepoint);
			int16_t below = static_cast<int16_t>(ch.bh - ch.bt);

			if (below > measured)
				measured = below;

			pos += length;
		}

		return measured;
	}

	Point<int16_t> FormatText::dimensions() const
	{
		return layout.get_dimensions();
	}
}
