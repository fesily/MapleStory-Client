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
#include "TextAnalyzer.h"

#include <cstdlib>

namespace
{
	// The number a payload starts with, the way the official analyzer reads it
	// (GetParameterNo at 0x97D620 is an atoi of everything behind the code), so a
	// payload such as "4001022:2" of #t gives up its item id as well
	int32_t payload_number(const std::string& payload)
	{
		size_t digits = 0;

		while (digits < payload.size() && payload[digits] >= '0' && payload[digits] <= '9')
			digits++;

		if (digits == 0)
			return 0;

		return std::atoi(payload.substr(0, digits).c_str());
	}

	// Whether a code stands for an object rather than a plain run of text: the item
	// and skill codes draw their canvas in front of the name, the rest only a canvas
	bool is_object_code(char code)
	{
		switch (code)
		{
			case 'i':
			case 'v':
			case 's':
			case 'f':
			case 'W':
			case 'B':
				return true;
			default:
				return false;
		}
	}

	// The canvas an object code draws, and the text in front of it (the name codes of
	// the items and skills carry one, the plain canvases do not)
	ms::Texture resolve_object(char code, int32_t id, const std::string& body, const ms::textformat::Resolver& resolver)
	{
		switch (code)
		{
			case 'i':
			case 'v':
				return resolver.item_icon(id);
			case 's':
				return resolver.skill_icon(id);
			case 'f':
				return resolver.ui_image(body);
			case 'W':
				// The icon of a quest summary entry (UIQuestInfo, CTextAnalyzer's #W)
				return resolver.ui_image("UI/UIWindow2.img/Quest/quest_info/summary_icon/" + body);
			case 'B':
			{
				// The bar of a progress, the official analyzer keeps it in [10, 100]
				int32_t percent = id < 10 ? 10 : id > 100 ? 100 : id;

				return resolver.ui_image("UI/Login.img/Notice/Loading/bar/" + std::to_string(percent));
			}
			default:
				return ms::Texture();
		}
	}

	// The text a name code resolves to. The codes which are missing here need data
	// this client does not keep yet (#a #u #M #D #Q #R #j), or stand for an object
	// rather than a name (#B #f #W #_), so they produce no element.
	std::string resolve_name(char code, int32_t id, const ms::textformat::Resolver& resolver)
	{
		switch (code)
		{
			case 't':
			case 'z':
			case 'i':
			case 'v':
				return resolver.item_name(id);
			case 'c':
				// #c<itemid># is the number of the item in the player's inventory, not
				// its name (the format table of the client lists it as such)
				return std::to_string(resolver.item_count(id));
			case 'o':
				return resolver.mob_name(id);
			case 'p':
			case '@':
				return resolver.npc_name(id);
			case 'm':
				return resolver.map_name(id);
			case 'q':
			case 's':
				return resolver.skill_name(id);
			case 'y':
				return resolver.quest_name(id);
			case 'h':
				return resolver.player_name();
			default:
				return std::string();
		}
	}
}

namespace ms
{
	namespace textformat
	{
		AnalyzedText analyze(const std::string& text, Text::Font font, Color::Name color, const Resolver& resolver, Mode mode)
		{
			AnalyzedText analyzed;

			// The state the codes of the text switch while it is walked
			Text::Font element_font = font;
			Color::Name element_color = color;
			int16_t select = -1;
			int16_t line = 0;
			// Whether the element behind the one being read came from plain characters:
			// the official analyzer keeps a whole run of them together, while the
			// tokenizer cuts it at every space a word can be wrapped at
			bool joined = false;

			size_t index = 0;

			while (index < text.size())
			{
				size_t length = 0;
				Unit unit = classify(text, index, length, mode);

				switch (unit)
				{
					case Unit::PLAIN:
					{
						// joined implies that the element behind this unit is the one it
						// continues
						if (joined
							&& analyzed.elements.back().color == element_color
							&& analyzed.elements.back().font == element_font
							&& analyzed.elements.back().select == select
							&& analyzed.elements.back().line == line)
						{
							Element& back = analyzed.elements.back();

							back.text.append(text, index, length);
							back.last = index + length;
						}
						else
						{
							Element element;
							element.font = element_font;
							element.color = element_color;
							element.text = text.substr(index, length);
							element.line = line;
							element.select = select;
							element.first = index;
							element.last = index + length;

							analyzed.elements.push_back(std::move(element));
						}

						joined = true;
						break;
					}
					case Unit::LITERAL:
					{
						// A '#' no code follows stays an element of its own, the way the
						// official analyzer phrases it; the '\\' of a description stands
						// for one backslash
						Element element;
						element.font = element_font;
						element.color = element_color;

						if (mode == Mode::DESCRIPTION && length == 2 && text[index] == '\\')
							element.text = "\\";
						else
							element.text = text.substr(index, length);
						element.line = line;
						element.select = select;
						element.first = index;
						element.last = index + length;

						analyzed.elements.push_back(std::move(element));
						joined = false;
						break;
					}
					case Unit::STYLE:
					{
						joined = false;

						switch (text[index + 1])
						{
							// #b - Blue text
							case 'b':
								element_color = Color::Name::PERSIANGREEN;
								break;
							// #d - Purple text
							case 'd':
								element_color = Color::Name::VIOLET;
								break;
							// #e - Bold text
							case 'e':
								element_font = Text::bold(element_font);
								break;
							// #g - Green text
							case 'g':
								element_color = Color::Name::GREEN;
								break;
							// #k - Black text
							case 'k':
								element_color = Color::Name::DARKGREY;
								break;
							// #l - Ends the list of items in the selection
							case 'l':
								select = -1;
								break;
							// #n - Normal text (Removes bold)
							case 'n':
								element_font = Text::normal(element_font);
								break;
							// #r - Red text
							case 'r':
								element_color = Color::Name::RED;
								break;
						}

						break;
					}
					case Unit::MARKER:
					{
						joined = false;

						// ## is a pair the official parser swallows without a trace
						if (text[index + 1] == '#')
							break;

						Element element;
						element.kind = Element::Kind::MARKER;
						element.code = length > 1 ? text[index + 1] : text[index];
						element.line = line;
						element.select = select;
						element.first = index;
						element.last = index + length;

						analyzed.elements.push_back(std::move(element));
						break;
					}
					case Unit::PAYLOAD:
					{
						joined = false;

						char code = text[index + 1];
						std::string body = payload(text, index, length);
						int32_t id = payload_number(body);

						if (code == 'L')
						{
							// The entries a sendSimple dialog offers: #L<n># starts the
							// entry n, every element behind it belongs to it until #l
							Element element;
							element.kind = Element::Kind::SELECT;
							element.code = code;
							element.id = id;
							element.line = line;
							element.select = static_cast<int16_t>(id);
							element.first = index;
							element.last = index + length;
							// The marker an entry is drawn behind: UtilDlgEx draws dot0
							// before every entry and dot1 before the one under the cursor
							element.icon = resolver.ui_image("UI/UIWindow2.img/UtilDlgEx/dot0");

							analyzed.elements.push_back(std::move(element));
							select = static_cast<int16_t>(id);
							break;
						}

						Texture icon = resolve_object(code, id, body, resolver);
						std::string name = resolve_name(code, id, resolver);

						// A name code whose data the client does not have produces
						// nothing; an object code keeps its element, the way the official
						// analyzer emits one even for a canvas it could not load
						if (name.empty() && !is_object_code(code))
							break;

						Element element;
						element.font = element_font;
						element.color = element_color;
						element.text = std::move(name);
						element.icon = std::move(icon);
						element.code = code;
						element.id = id;
						element.line = line;
						element.select = select;
						element.first = index;
						element.last = index + length;

						if (is_object_code(code))
							element.kind = Element::Kind::OBJECT;

						analyzed.elements.push_back(std::move(element));
						break;
					}
					case Unit::BREAK:
					{
						joined = false;

						// The official analyzer marks the element the line ends behind
						// and counts the line up, even when it starts one
						if (!analyzed.elements.empty())
							analyzed.elements.back().line_break = true;

						line++;
						break;
					}
				}

				index += length;
			}

			analyzed.lines = static_cast<int16_t>(line + (analyzed.elements.empty() ? 0 : 1));

			return analyzed;
		}
	}
}
