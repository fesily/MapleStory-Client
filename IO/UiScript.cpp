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
#include "UiScript.h"

#include "UI.h"
#include "UIElement.h"

#include "../Util/DebugConsole.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace ms
{
	namespace ui_script
	{
		namespace
		{
			const char* WHITESPACE = " \t\r\n";

			// The name of an element type, so that a listing says which screen it is
			const char* type_name(UIElement::Type type)
			{
				switch (type)
				{
					case UIElement::Type::NONE:
						return "NONE";
					case UIElement::Type::START:
						return "START";
					case UIElement::Type::LOGIN:
						return "LOGIN";
					case UIElement::Type::TOS:
						return "TOS";
					case UIElement::Type::GENDER:
						return "GENDER";
					case UIElement::Type::WORLDSELECT:
						return "WORLDSELECT";
					case UIElement::Type::REGION:
						return "REGION";
					case UIElement::Type::CHARSELECT:
						return "CHARSELECT";
					case UIElement::Type::LOGINWAIT:
						return "LOGINWAIT";
					case UIElement::Type::RACESELECT:
						return "RACESELECT";
					case UIElement::Type::CLASSCREATION:
						return "CLASSCREATION";
					case UIElement::Type::SOFTKEYBOARD:
						return "SOFTKEYBOARD";
					case UIElement::Type::LOGINNOTICE:
						return "LOGINNOTICE";
					case UIElement::Type::LOGINNOTICE_CONFIRM:
						return "LOGINNOTICE_CONFIRM";
					case UIElement::Type::STATUSMESSENGER:
						return "STATUSMESSENGER";
					case UIElement::Type::STATUSBAR:
						return "STATUSBAR";
					case UIElement::Type::CHATBAR:
						return "CHATBAR";
					case UIElement::Type::BUFFLIST:
						return "BUFFLIST";
					case UIElement::Type::NOTICE:
						return "NOTICE";
					case UIElement::Type::NPCTALK:
						return "NPCTALK";
					case UIElement::Type::SHOP:
						return "SHOP";
					case UIElement::Type::STATSINFO:
						return "STATSINFO";
					case UIElement::Type::ITEMINVENTORY:
						return "ITEMINVENTORY";
					case UIElement::Type::EQUIPINVENTORY:
						return "EQUIPINVENTORY";
					case UIElement::Type::SKILLBOOK:
						return "SKILLBOOK";
					case UIElement::Type::QUESTLOG:
						return "QUESTLOG";
					case UIElement::Type::WORLDMAP:
						return "WORLDMAP";
					case UIElement::Type::USERLIST:
						return "USERLIST";
					case UIElement::Type::MINIMAP:
						return "MINIMAP";
					case UIElement::Type::CHANNEL:
						return "CHANNEL";
					case UIElement::Type::CHAT:
						return "CHAT";
					case UIElement::Type::CHATRANK:
						return "CHATRANK";
					case UIElement::Type::JOYPAD:
						return "JOYPAD";
					case UIElement::Type::EVENT:
						return "EVENT";
					case UIElement::Type::KEYCONFIG:
						return "KEYCONFIG";
					case UIElement::Type::OPTIONMENU:
						return "OPTIONMENU";
					case UIElement::Type::QUIT:
						return "QUIT";
					case UIElement::Type::CHARINFO:
						return "CHARINFO";
					case UIElement::Type::CASHSHOP:
						return "CASHSHOP";
					case UIElement::Type::REVIVE:
						return "REVIVE";
					default:
						return "?";
				}
			}

			// The element in front, which is the last active one in the order the state
			// keeps them: the same one the cursor is handed to (UIStateLogin::get_front)
			UIElement* front()
			{
				UIElement* result = nullptr;

				for (int32_t type = UIElement::Type::NONE + 1; type < UIElement::Type::NUM_TYPES; type++)
				{
					UIElement* element = UI::get().get_element(static_cast<UIElement::Type>(type));

					if (element && element->is_active())
						result = element;
				}

				return result;
			}

			// Split off the first word of a line; what follows keeps its own spaces, so
			// that a value can be handed over as it was typed
			void split(const std::string& line, std::string& head, std::string& rest)
			{
				size_t begin = line.find_first_not_of(WHITESPACE);

				if (begin == std::string::npos)
				{
					head.clear();
					rest.clear();
					return;
				}

				size_t end = line.find_first_of(WHITESPACE, begin);

				if (end == std::string::npos)
				{
					head = line.substr(begin);
					rest.clear();
					return;
				}

				head = line.substr(begin, end - begin);

				size_t restbegin = line.find_first_not_of(WHITESPACE, end);

				rest = restbegin == std::string::npos ? std::string() : line.substr(restbegin);
			}

			bool is_number(const std::string& text)
			{
				if (text.empty())
					return false;

				for (char c : text)
				{
					if (c < '0' || c > '9')
						return false;
				}

				return true;
			}

			void list(UIElement* element)
			{
				std::vector<UIElement::Offer> offers;
				element->describe(offers);

				std::cout << type_name(element->get_type()) << " offers " << offers.size() << ":" << std::endl;

				for (const UIElement::Offer& offer : offers)
				{
					switch (offer.kind)
					{
						case UIElement::Offer::Kind::FIELD:
							std::cout << "  field " << offer.name;
							break;
						case UIElement::Offer::Kind::ACTION:
							std::cout << "  action " << offer.name;
							break;
						case UIElement::Offer::Kind::BUTTON:
							std::cout << "  button " << offer.name;
							break;
					}

					if (!offer.hint.empty())
						std::cout << ": " << offer.hint;

					std::cout << std::endl;
				}
			}

			// ui [list|set <field> <value>|do <action>|click <id>]
			void command_ui(const std::string& args)
			{
				std::string command;
				std::string rest;

				split(args, command, rest);

				UIElement* element = front();

				if (!element)
				{
					std::cout << "no element is in front" << std::endl;
					return;
				}

				if (command.empty() || command == "list")
				{
					list(element);
					return;
				}

				if (command == "set")
				{
					std::string name;
					std::string value;

					split(rest, name, value);

					if (name.empty() || value.empty())
					{
						std::cout << "usage: ui set <field> <value>" << std::endl;
						return;
					}

					if (element->set_field(name, value))
					{
						// The value itself is not echoed: the console holds it already
						std::cout << "set " << name << std::endl;
					}
					else
					{
						std::cout << type_name(element->get_type()) << " does not take " << name << std::endl;

						list(element);
					}

					return;
				}

				if (command == "do")
				{
					if (rest.empty())
					{
						std::cout << "usage: ui do <action>" << std::endl;
						return;
					}

					if (element->trigger(rest))
						std::cout << "did " << rest << std::endl;
					else
					{
						std::cout << type_name(element->get_type()) << " does not run " << rest << std::endl;

						list(element);
					}

					return;
				}

				if (command == "click")
				{
					if (!is_number(rest))
					{
						std::cout << "usage: ui click <button id>" << std::endl;
						return;
					}

					uint16_t id = static_cast<uint16_t>(std::atoi(rest.c_str()));

					if (element->press_button(id))
						std::cout << "clicked " << id << std::endl;
					else
					{
						std::cout << type_name(element->get_type()) << " has no button " << id << std::endl;

						list(element);
					}

					return;
				}

				std::cout << "usage: ui [list|set <field> <value>|do <action>|click <id>]" << std::endl;
			}
		}

		void register_commands()
		{
			debug_console::add({
				{ "ui", "[list|set <field> <value>|do <action>|click <id>]",
					"work the element in front: list what it takes, set a field, run an action", command_ui },
			});
		}
	}
}
