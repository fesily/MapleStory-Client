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
#include "MapTooltip.h"

#include "../../Util/Misc.h"

#include <algorithm>
#include <unordered_map>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	// TODO: Get number of available quests
	MapTooltip::MapTooltip() : parent(UIElement::Type::NONE), title(""), description(""), fillwidth(0), fillheight(0)
	{
		nl::node Frame = nl::nx::UI["UIToolTip.img"]["Item"]["Frame2"];
		nl::node WorldMap = nl::nx::UI["UIWindow2.img"]["ToolTip"]["WorldMap"];

		frame = Frame;
		cover = Frame["cover"];
		Mob = WorldMap["Mob"];
		Npc = WorldMap["Npc"];

		// TODO: Get list of where party members are
		//Party = WorldMap["Party"];
	}

	void MapTooltip::draw(Point<int16_t> position) const
	{
		if (title_label.empty())
			return;

		int16_t max_width = Constants::Constants::get().get_viewwidth();
		int16_t max_height = Constants::Constants::get().get_viewheight();
		int16_t cur_width = position.x() + fillwidth + 14 + 9 + 17;
		int16_t cur_height = position.y() + fillheight + 14 + 4 + 6;
		int16_t adj_x = cur_width - max_width;
		int16_t adj_y = cur_height - max_height;

		if (adj_x > 0)
			position.shift_x(adj_x * -1);

		if (adj_y > 0)
			position.shift_y(adj_y * -1);

		// Shift everything so the cursor is in the top left corner
		position.shift(Point<int16_t>(20, -3));

		switch (parent)
		{
			case UIElement::Type::WORLDMAP:
				draw_worldmap(position);
				break;
			case UIElement::Type::MINIMAP:
				draw_minimap(position);
				break;
			default:
				break;
		}
	}

	void MapTooltip::set_title(UIElement::Type p, std::string t, bool bolded)
	{
		if (parent == p && title == t)
			return;

		parent = p;
		title = t;

		if (title.empty())
			return;

		switch (parent)
		{
			case UIElement::Type::WORLDMAP:
				set_worldmap_title(bolded);
				break;
			case UIElement::Type::MINIMAP:
				set_minimap_title(bolded);
				break;
			default:
				break;
		}
	}

	void MapTooltip::set_desc(std::string d)
	{
		if (description == d)
			return;

		description = d;

		if (description.empty())
			return;

		switch (parent)
		{
			case UIElement::Type::WORLDMAP:
				set_worldmap_desc();
				break;
			case UIElement::Type::MINIMAP:
				set_minimap_desc();
				break;
			default:
				break;
		}
	}

	void MapTooltip::set_mapid(int32_t mapid, bool portal)
	{
		std::unordered_map<int64_t, std::pair<std::string, std::string>> life = NxHelper::Map::get_life_on_map(mapid);

		switch (parent)
		{
			case UIElement::Type::WORLDMAP:
				set_worldmap_mapid(life, portal);
				break;
			case UIElement::Type::MINIMAP:
				set_minimap_mapid(life, portal);
				break;
			default:
				break;
		}
	}

	void MapTooltip::reset()
	{
		parent = UIElement::Type::NONE;

		title = "";
		title_label.change_text("");

		description = "";
		desc_label.change_text("");

		clear_life_labels();

		fillwidth = 0;
		fillheight = 0;
	}

	void MapTooltip::clear_life_labels()
	{
		for (uint8_t i = 0; i < MAX_LIFE; i++)
		{
			mob_labels[i].change_text("");
			npc_labels[i].change_text("");
		}
	}

	void MapTooltip::draw_worldmap(Point<int16_t> position) const
	{
		int16_t width = fillwidth - 8;
		int16_t halfwidth = width / 2;

		frame.draw(position + Point<int16_t>(halfwidth, fillheight) + Point<int16_t>(14, 14), width, fillheight);
		cover.draw(position + Point<int16_t>(0, 1));

		position.shift(Point<int16_t>(9, 4));

		title_label.draw(position + Point<int16_t>(halfwidth + 5, 0));

		if (!desc_label.empty())
		{
			position.shift_y(title_label.height() + 5);

			desc_label.draw(position + Point<int16_t>(-1, 0));
			position.shift_y(desc_label.height());
		}
		else
		{
			position.shift_y(title_label.height() - 2);
		}

		for (uint8_t i = 0; i < MAX_LIFE; i++)
		{
			Text mob = mob_labels[i];

			if (!mob.empty())
			{
				if (i == 0) {
					separator.draw(position + SEPARATOR_ADJ);
					position.shift_y(10);

					Mob.draw(position + LIFE_ICON_ADJ);
				}

				mob.draw(position + LIFE_LABEL_ADJ);
				position.shift_y(mob.height() + 1);
			}
		}

		if (!mob_labels->empty() && !npc_labels->empty())
			position.shift_y(-2);

		for (uint8_t i = 0; i < MAX_LIFE; i++)
		{
			Text npc = npc_labels[i];

			if (!npc.empty())
			{
				if (i == 0) {
					separator.draw(position + SEPARATOR_ADJ);
					position.shift_y(10);

					Npc.draw(position + LIFE_ICON_ADJ);
				}

				npc.draw(position + LIFE_LABEL_ADJ);
				position.shift_y(npc.height() + 1);
			}
		}
	}

	void MapTooltip::draw_minimap(Point<int16_t> position) const
	{
		bool desc_empty = desc_label.empty();

		if (desc_empty && mob_labels->empty() && npc_labels->empty())
		{
			int16_t width = fillwidth - 12;
			int16_t halfwidth = width / 2;

			int16_t height = fillheight - title_label.height();

			frame.draw(position + Point<int16_t>(halfwidth, height) + Point<int16_t>(14, 14), width, height);

			position.shift(Point<int16_t>(9, 2));

			title_label.draw(position + Point<int16_t>(halfwidth + 5, 0));
		}
		else
		{
			int16_t width = fillwidth - (!desc_empty ? 12 : 8);
			int16_t halfwidth = width / 2;

			int16_t height = fillheight - 2;

			frame.draw(position + Point<int16_t>(halfwidth, height) + Point<int16_t>(14, 14), width, height);
			cover.draw(position + Point<int16_t>(0, 1));

			if (!desc_empty)
				position.shift(Point<int16_t>(7, 2));
			else
				position.shift(Point<int16_t>(9, 4));

			title_label.draw(position + Point<int16_t>(halfwidth + 7, 0));
			position.shift_y(title_label.height());

			if (!desc_empty)
			{
				separator.draw(position + SEPARATOR_ADJ - Point<int16_t>(0, 8));

				desc_label.draw(position + Point<int16_t>(halfwidth + 7, 0));
				position.shift_y(desc_label.height());
			}

			for (uint8_t i = 0; i < MAX_LIFE; i++)
			{
				Text mob = mob_labels[i];

				if (!mob.empty())
				{
					if (i == 0) {
						separator.draw(position + SEPARATOR_ADJ - Point<int16_t>(0, 2));
						position.shift_y(8);

						Mob.draw(position + LIFE_ICON_ADJ);
					}

					mob.draw(position + LIFE_LABEL_ADJ);
					position.shift_y(mob.height() + 1);
				}
			}

			for (uint8_t i = 0; i < MAX_LIFE; i++)
			{
				Text npc = npc_labels[i];

				if (!npc.empty())
				{
					if (i == 0) {
						separator.draw(position + SEPARATOR_ADJ - Point<int16_t>(0, 2));
						position.shift_y(8);

						Npc.draw(position + LIFE_ICON_ADJ);
					}

					npc.draw(position + LIFE_LABEL_ADJ);
					position.shift_y(npc.height() + 1);
				}
			}
		}
	}

	void MapTooltip::set_worldmap_title(bool bolded)
	{
		fillwidth = WORLDMAP_WIDTH;
		title_label = Text(bolded ? Text::Font::A12B : Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE, title);

		int16_t width = title_label.width();
		int16_t height = title_label.height();

		if (width > fillwidth)
			fillwidth = width;

		separator = ColorLine(fillwidth, Color::Name::WHITE, 0.40f, false);

		if (height > fillheight)
			fillheight = height;
	}

	void MapTooltip::set_minimap_title(bool)
	{
		title_label = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE, title);

		int16_t width = title_label.width();
		int16_t height = title_label.height();

		if (width > fillwidth)
			fillwidth = width;

		separator = ColorLine(fillwidth, Color::Name::WHITE, 0.40f, false);

		if (height > fillheight)
			fillheight = height;
	}

	void MapTooltip::set_worldmap_desc()
	{
		desc_label = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, description, fillwidth);

		int16_t width = desc_label.width();
		int16_t height = desc_label.height();

		if (width > fillwidth)
			fillwidth = width;

		separator = ColorLine(fillwidth, Color::Name::WHITE, 0.40f, false);

		if (height > fillheight)
			fillheight = height;
	}

	void MapTooltip::set_minimap_desc()
	{
		desc_label = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE, description);

		int16_t width = desc_label.width();
		int16_t height = desc_label.height();

		if (width > fillwidth)
			fillwidth = width;

		separator = ColorLine(fillwidth, Color::Name::WHITE, 0.40f, false);

		if (height > fillheight)
			fillheight = height;
	}

	void MapTooltip::set_worldmap_mapid(std::unordered_map<int64_t, std::pair<std::string, std::string>> life, bool)
	{
		int32_t m = 0;
		int32_t n = 0;
		bool desc_empty = desc_label.empty();

		// The box is sized from the content it holds: this runs for every frame the cursor
		// rests on a spot and the rows used to be added to the height the tooltip before them
		// left behind, which made the box grow as long as it stayed up. The width and height
		// the title alone asks for are the ones a tooltip starts from when it is shown fresh.
		fillwidth = WORLDMAP_WIDTH;
		fillheight = title_label.height();

		clear_life_labels();

		if (title_label.width() > fillwidth)
			fillwidth = title_label.width();

		if (!desc_empty)
		{
			fillwidth = std::max(fillwidth, desc_label.width());
			fillheight = std::max(fillheight, desc_label.height());

			fillheight += title_label.height() + 7;
		}

		for (auto& l : life)
		{
			auto& life_object = l.second;

			if (life_object.first == "m" && m < MAX_LIFE)
			{
				mob_labels[m] = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::CHARTREUSE, life_object.second);
				fillheight += mob_labels->height() + 1;

				m++;
			}
			else if (life_object.first == "n" && n < MAX_LIFE)
			{
				npc_labels[n] = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::MALIBU, life_object.second);

				if (m > 0 && n == 0)
					fillheight += 8;

				fillheight += npc_labels->height() + 1;

				n++;
			}
		}

		fillheight -= 3;

		if (desc_empty && m == 0 && n == 0)
			fillheight -= 8;
	}

	void MapTooltip::set_minimap_mapid(std::unordered_map<int64_t, std::pair<std::string, std::string>> life, bool portal)
	{
		int32_t m = 0;
		int32_t n = 0;
		bool desc_empty = desc_label.empty();

		// Sizes the box from the content it holds, the same way set_worldmap_mapid() does:
		// the rows are not added to the height the tooltip before them left behind
		fillwidth = title_label.width();
		fillheight = title_label.height();

		clear_life_labels();

		if (!desc_empty)
		{
			fillwidth = std::max(fillwidth, desc_label.width());
			fillheight = std::max(fillheight, desc_label.height());

			if (portal)
				fillheight += title_label.height() + 7;
		}

		for (auto& l : life)
		{
			auto& life_object = l.second;

			if (life_object.first == "m" && m < MAX_LIFE)
			{
				mob_labels[m] = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::CHARTREUSE, life_object.second);

				int16_t width = mob_labels[m].width() + Mob.width() + 8;

				if (width > fillwidth)
					fillwidth = width;

				separator = ColorLine(fillwidth, Color::Name::WHITE, 0.40f, false);
				fillheight += mob_labels->height() + 1;

				m++;
			}
			else if (life_object.first == "n" && n < MAX_LIFE)
			{
				npc_labels[n] = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::MALIBU, life_object.second);

				int16_t width = npc_labels[n].width() + Npc.width() + 8;

				if (width > fillwidth)
					fillwidth = width;

				separator = ColorLine(fillwidth, Color::Name::WHITE, 0.40f, false);

				if (m > 0 && n == 0)
					fillheight += 8;

				fillheight += npc_labels->height() + 1;

				n++;
			}
		}

		if (portal)
		{
			fillheight -= 1;

			if (desc_empty && m == 0 && n == 0)
				fillheight -= 8;
		}
	}
}