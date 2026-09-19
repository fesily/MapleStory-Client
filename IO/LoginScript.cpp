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
#include "LoginScript.h"

#include "UI.h"
#include "UITypes/UILoginNotice.h"

#include "../Configuration.h"
#include "../Util/DebugConsole.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ms
{
	namespace login_script
	{
		namespace
		{
			// How long a screen may stay without the script getting anywhere, in
			// milliseconds: a server which never answers must not hang the script
			const int64_t TIMEOUT = 20000;

			// The screen the script acts on. Which stage of the login is due is read
			// from the screen in front, not counted, so that a click of the player and
			// the script stay in step with each other.
			enum class Screen
			{
				None,
				Login,
				WorldSelect,
				CharSelect,
				SoftKey,
				Notice,
				Terms,
				Gender,
				Wait,
				Game
			};

			// The value a prompt is waiting for
			enum class Ask
			{
				None,
				Account,
				Password,
				World,
				Channel,
				Character,
				Pic
			};

			struct Script
			{
				bool running = false;
				// A prompt is out and the line it gets decides what happens next
				bool asked = false;
				Ask ask = Ask::None;
				// The screen the last step was taken on and whether it was taken
				Screen screen = Screen::None;
				bool acted = false;
				int64_t changed = 0;

				// What the command or a prompt already provided. The value is handed to
				// the screen when the step is taken, which is where it is checked.
				bool have_account = false;
				bool have_password = false;
				bool have_world = false;
				bool have_channel = false;
				bool have_character = false;
				bool have_pic = false;

				std::string account;
				std::string password;
				std::string world;
				std::string channel;
				std::string character;
				std::string pic;
			};

			Script script;

			int64_t now()
			{
				using namespace std::chrono;

				return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
			}

			// Whether the screen is in front, i.e. exists and was not taken down
			bool up(UIElement::Type type)
			{
				UIElement* element = UI::get().get_element(type);

				return element && element->is_active();
			}

			// The notice shares its element type with four other dialogs (UIElement.h
			// LOGINNOTICE), so which class the element is has to be asked, not assumed
			UILoginNotice* notice()
			{
				UILoginNotice* element = dynamic_cast<UILoginNotice*>(UI::get().get_element(UIElement::Type::LOGINNOTICE));

				return element && element->is_active() ? element : nullptr;
			}

			// Ask a screen to take a value, or to run one of its actions. Which values
			// it takes is its own to check, so the caller does not have to know the
			// list behind the element (UIElement::set_field).
			bool set_field(UIElement::Type type, const std::string& name, const std::string& value)
			{
				UIElement* element = UI::get().get_element(type);

				return element && element->set_field(name, value);
			}

			bool trigger(UIElement::Type type, const std::string& action)
			{
				UIElement* element = UI::get().get_element(type);

				return element && element->trigger(action);
			}

			// The values a screen takes, which a prompt prints so that the answer can be
			// looked up. An empty name prints every field of it.
			void print_fields(UIElement::Type type, const std::string& name = std::string())
			{
				UIElement* element = UI::get().get_element(type);

				if (!element)
					return;

				std::vector<UIElement::Offer> offers;
				element->describe(offers);

				for (const UIElement::Offer& offer : offers)
				{
					if (offer.kind != UIElement::Offer::Kind::FIELD)
						continue;

					if (!name.empty() && offer.name != name)
						continue;

					std::cout << "  " << offer.name;

					if (!offer.hint.empty())
						std::cout << ": " << offer.hint;

					std::cout << std::endl;
				}
			}

			// The screen a prompt asks about and the field it fills in
			UIElement::Type field_screen(Ask value)
			{
				switch (value)
				{
					case Ask::Account:
					case Ask::Password:
						return UIElement::Type::LOGIN;
					case Ask::World:
					case Ask::Channel:
						return UIElement::Type::WORLDSELECT;
					case Ask::Character:
						return UIElement::Type::CHARSELECT;
					case Ask::Pic:
						return UIElement::Type::SOFTKEYBOARD;
					default:
						return UIElement::Type::NONE;
				}
			}

			const char* field_name(Ask value)
			{
				switch (value)
				{
					case Ask::Account:
						return "account";
					case Ask::Password:
						return "password";
					case Ask::World:
						return "world";
					case Ask::Channel:
						return "channel";
					case Ask::Character:
						return "character";
					case Ask::Pic:
						return "pic";
					default:
						return "";
				}
			}

			// What a notice says, for the ones the login can be answered with. The
			// numbers are the UILoginNotice::Message values the handlers pass on.
			const char* notice_text(uint16_t message)
			{
				switch (message)
				{
					case UILoginNotice::Message::WRONG_PASSWORD:
						return "the password was refused";
					case UILoginNotice::Message::NOT_REGISTERED:
						return "the account is not registered";
					case UILoginNotice::Message::BLOCKED_ID:
						return "the account or this address is blocked";
					case UILoginNotice::Message::ALREADY_LOGGED_IN:
						return "the account is logged in already";
					case UILoginNotice::Message::TOO_MANY_REQUESTS:
						return "too many login attempts";
					case UILoginNotice::Message::TROUBLE_LOGGING_IN:
						return "the server could not log the account in";
					case UILoginNotice::Message::UNABLE_TO_LOGIN_WITH_IP:
						return "too many clients from this address";
					case UILoginNotice::Message::CANNOT_ACCESS_ACCOUNT:
						return "the account cannot be accessed";
					case UILoginNotice::Message::WRONG_GATEWAY:
						return "the address does not belong to the session";
					case UILoginNotice::Message::PASSWORD_IS_INCORRECT:
					case UILoginNotice::Message::INCORRECT_PIC:
						return "the PIC was refused";
					case UILoginNotice::Message::POPULATION_TOO_HIGH:
						return "the world is full";
					case UILoginNotice::Message::SELECT_A_CHANNEL:
						return "no channel was picked";
					case UILoginNotice::Message::UNABLE_TO_CONNECT:
						return "the server could not be reached";
					case UILoginNotice::Message::UNKNOWN_ERROR:
						return "the server answered with an error";
					default:
						return "the server showed a notice this script does not know";
				}
			}

			// The command line takes the values as they are typed; only the words the
			// commands need are split off, a value may not hold a space
			std::vector<std::string> tokens(const std::string& args)
			{
				std::istringstream stream(args);
				std::vector<std::string> words;
				std::string word;

				while (stream >> word)
					words.emplace_back(word);

				return words;
			}

			void ask(Ask value);
			void answer(const std::string& line);

			void stop(const std::string& reason)
			{
				script.running = false;
				script.asked = false;
				script.ask = Ask::None;

				debug_console::cancel_await();

				std::cout << "login script stopped: " << reason << std::endl;
			}

			std::string prompt_text(Ask value)
			{
				switch (value)
				{
					case Ask::Account:
					{
						const std::string& saved = Setting<DefaultAccount>::get().load();

						return saved.empty() ? "account: " : "account [" + saved + "]: ";
					}
					case Ask::Password:
						return "password: ";
					case Ask::World:
						return "world: ";
					case Ask::Channel:
						return "channel: ";
					case Ask::Character:
						return "character: ";
					case Ask::Pic:
						return "pic: ";
					default:
						return std::string();
				}
			}

			void ask(Ask value)
			{
				script.ask = value;
				script.asked = true;
				// Answering a prompt takes as long as it takes, which is not the script
				// waiting for a screen and must not run into its timeout
				script.changed = now();

				// The account and the password are typed, the rest is picked from a list
				// which the screen knows and prints here
				if (value != Ask::Account && value != Ask::Password)
					print_fields(field_screen(value), field_name(value));

				std::cout << prompt_text(value) << std::flush;

				debug_console::await_line(answer);
			}

			void answer(const std::string& line)
			{
				script.asked = false;
				script.acted = false;
				script.changed = now();

				Ask value = script.ask;
				script.ask = Ask::None;

				switch (value)
				{
					case Ask::Account:
					{
						// The account the settings remember is taken for an empty answer
						std::string account = line.empty() ? Setting<DefaultAccount>::get().load() : line;

						if (account.empty())
						{
							ask(Ask::Account);
							return;
						}

						script.account = account;
						script.have_account = true;
						break;
					}
					case Ask::Password:
					{
						// Servers which do not use passwords accept an empty one
						if (line.empty() && !Setting<AllowEmptyPassword>::get().load())
						{
							ask(Ask::Password);
							return;
						}

						script.password = line;
						script.have_password = true;
						break;
					}
					case Ask::World:
						script.world = line;
						script.have_world = true;
						break;
					case Ask::Channel:
						script.channel = line;
						script.have_channel = true;
						break;
					case Ask::Character:
						script.character = line;
						script.have_character = true;
						break;
					case Ask::Pic:
						script.pic = line;
						script.have_pic = true;
						break;
					default:
						break;
				}
			}

			void act_login()
			{
				if (!up(UIElement::Type::LOGIN))
					return;

				if (!script.have_account)
				{
					ask(Ask::Account);
					script.acted = true;
					return;
				}

				if (!script.have_password)
				{
					ask(Ask::Password);
					script.acted = true;
					return;
				}

				if (!set_field(UIElement::Type::LOGIN, "account", script.account) ||
					!set_field(UIElement::Type::LOGIN, "password", script.password))
				{
					stop("the login screen does not take the account and the password");
					return;
				}

				std::cout << "logging in as " << script.account << std::endl;

				if (!trigger(UIElement::Type::LOGIN, "login"))
				{
					stop("the login screen does not log in");
					return;
				}

				script.acted = true;
			}

			void act_world()
			{
				if (!up(UIElement::Type::WORLDSELECT))
					return;

				if (!script.have_world)
				{
					ask(Ask::World);
					script.acted = true;
					return;
				}

				if (!set_field(UIElement::Type::WORLDSELECT, "world", script.world))
				{
					std::cout << "the world screen does not take '" << script.world << "'" << std::endl;

					ask(Ask::World);
					script.acted = true;
					return;
				}

				if (!script.have_channel)
				{
					ask(Ask::Channel);
					script.acted = true;
					return;
				}

				if (!set_field(UIElement::Type::WORLDSELECT, "channel", script.channel))
				{
					std::cout << "the world screen does not take channel '" << script.channel << "'" << std::endl;

					ask(Ask::Channel);
					script.acted = true;
					return;
				}

				std::cout << "picking world " << script.world << " channel " << script.channel << std::endl;

				if (!trigger(UIElement::Type::WORLDSELECT, "enter"))
				{
					stop("the world screen does not enter the world");
					return;
				}

				script.acted = true;
			}

			void act_character()
			{
				if (!up(UIElement::Type::CHARSELECT))
					return;

				if (!script.have_character)
				{
					ask(Ask::Character);
					script.acted = true;
					return;
				}

				if (!set_field(UIElement::Type::CHARSELECT, "character", script.character))
				{
					std::cout << "the character screen does not take '" << script.character << "'" << std::endl;

					ask(Ask::Character);
					script.acted = true;
					return;
				}

				std::cout << "picking character " << script.character << std::endl;

				if (!trigger(UIElement::Type::CHARSELECT, "select"))
				{
					stop("the character screen does not select");
					return;
				}

				script.acted = true;
			}

			void act_pic()
			{
				if (!up(UIElement::Type::SOFTKEYBOARD))
					return;

				if (!script.have_pic)
				{
					ask(Ask::Pic);
					script.acted = true;
					return;
				}

				if (!set_field(UIElement::Type::SOFTKEYBOARD, "pic", script.pic))
				{
					std::cout << "the softkey does not take that PIC" << std::endl;

					ask(Ask::Pic);
					script.acted = true;
					return;
				}

				std::cout << "typing the PIC" << std::endl;

				if (!trigger(UIElement::Type::SOFTKEYBOARD, "ok"))
				{
					stop("the softkey does not confirm");
					return;
				}

				script.acted = true;
			}

			void act_notice()
			{
				UILoginNotice* shown = notice();

				if (!shown)
					return;

				uint16_t message = shown->get_message();

				if (message == UILoginNotice::Message::PIC_REQ)
				{
					// The server wants a new PIC before the character is entered: the
					// 'yes' of the notice opens the softkey which types it in
					std::cout << "the server asks for a new PIC" << std::endl;

					if (!trigger(UIElement::Type::LOGINNOTICE, "yes"))
					{
						stop("the notice does not answer");
						return;
					}

					script.acted = true;
					return;
				}

				std::ostringstream reason;
				reason << notice_text(message) << " (notice " << message << ")";

				stop(reason.str());
			}

			Screen observe()
			{
				if (UI::get().get_state() != UI::State::LOGIN)
					return Screen::Game;

				if (notice())
					return Screen::Notice;

				if (up(UIElement::Type::SOFTKEYBOARD))
					return Screen::SoftKey;

				if (up(UIElement::Type::CHARSELECT))
					return Screen::CharSelect;

				// The world screen is filled while the login screen is still up and only
				// becomes usable when it is taken down, so the login screen comes first
				if (up(UIElement::Type::LOGIN))
					return Screen::Login;

				if (up(UIElement::Type::WORLDSELECT))
					return Screen::WorldSelect;

				if (up(UIElement::Type::TOS))
					return Screen::Terms;

				if (up(UIElement::Type::GENDER))
					return Screen::Gender;

				return Screen::Wait;
			}

			void act(Screen screen)
			{
				switch (screen)
				{
					case Screen::Login:
						act_login();
						break;
					case Screen::WorldSelect:
						act_world();
						break;
					case Screen::CharSelect:
						act_character();
						break;
					case Screen::SoftKey:
						act_pic();
						break;
					case Screen::Notice:
						act_notice();
						break;
					case Screen::Terms:
						stop("the server wants the terms of service accepted, which this script does not do");
						break;
					case Screen::Gender:
						stop("the server wants the gender picked, which this script does not do");
						break;
					default:
						break;
				}
			}

			void start()
			{
				script.running = true;
				script.screen = Screen::None;
				script.acted = false;
				script.changed = now();

				std::cout << "login script started; 'cancel' gives the mouse back" << std::endl;
			}

			// A command which says what a prompt is waiting for answers it instead
			void drop_prompt()
			{
				if (!script.asked)
					return;

				script.asked = false;
				script.ask = Ask::None;

				debug_console::cancel_await();
			}

			// The notice of an earlier attempt stays on screen and would stop the next
			// one right away; pressing its 'yes' is what a player does before retrying
			void dismiss_notice()
			{
				UILoginNotice* shown = notice();

				if (!shown)
					return;

				std::cout << "closing the notice of the last attempt" << std::endl;

				shown->trigger("yes");
			}

			// login [account] [password] [world] [channel] [character] [pic]
			void command_login(const std::string& args)
			{
				if (UI::get().get_state() != UI::State::LOGIN)
				{
					std::cout << "the client is not on the login screens" << std::endl;
					return;
				}

				drop_prompt();
				dismiss_notice();

				if (script.running)
					std::cout << "the login script starts over" << std::endl;

				script = Script();

				std::vector<std::string> words = tokens(args);

				if (words.size() > 0)
				{
					script.account = words[0];
					script.have_account = true;
				}

				if (words.size() > 1)
				{
					script.password = words[1];
					script.have_password = true;
				}

				if (words.size() > 2)
				{
					script.world = words[2];
					script.have_world = true;
				}

				if (words.size() > 3)
				{
					script.channel = words[3];
					script.have_channel = true;
				}

				if (words.size() > 4)
				{
					script.character = words[4];
					script.have_character = true;
				}

				if (words.size() > 5)
				{
					script.pic = words[5];
					script.have_pic = true;
				}

				start();
			}

			// world <world> [channel]
			void command_world(const std::string& args)
			{
				if (UI::get().get_state() != UI::State::LOGIN)
				{
					std::cout << "the client is not on the login screens" << std::endl;
					return;
				}

				std::vector<std::string> words = tokens(args);

				if (words.empty())
				{
					print_fields(UIElement::Type::WORLDSELECT);

					std::cout << "usage: world <world> [channel]" << std::endl;
					return;
				}

				if (!script.running && !up(UIElement::Type::WORLDSELECT))
				{
					std::cout << "the world screen is not up; start with 'login <account> <password>'" << std::endl;
					return;
				}

				drop_prompt();

				script.world = words[0];
				script.have_world = true;

				if (words.size() > 1)
				{
					script.channel = words[1];
					script.have_channel = true;
				}

				script.acted = false;

				if (!script.running)
					start();
			}

			// char <slot|name|#id> [pic]
			void command_char(const std::string& args)
			{
				if (UI::get().get_state() != UI::State::LOGIN)
				{
					std::cout << "the client is not on the login screens" << std::endl;
					return;
				}

				std::vector<std::string> words = tokens(args);

				if (words.empty())
				{
					print_fields(UIElement::Type::CHARSELECT);

					std::cout << "usage: char <slot|name|#id> [pic]" << std::endl;
					return;
				}

				if (!script.running && !up(UIElement::Type::CHARSELECT))
				{
					std::cout << "the character screen is not up; start with 'login <account> <password>'" << std::endl;
					return;
				}

				drop_prompt();

				script.character = words[0];
				script.have_character = true;

				if (words.size() > 1)
				{
					script.pic = words[1];
					script.have_pic = true;
				}

				script.acted = false;

				if (!script.running)
					start();
			}

			// cancel
			void command_cancel(const std::string&)
			{
				cancel();
			}
		}

		void register_commands()
		{
			debug_console::add({
				{ "login", "[account] [password] [world] [channel] [character] [pic]",
					"log in and enter the game by working the login screens", command_login },
				{ "world", "<world> [channel]",
					"pick the world and the channel of the world screen", command_world },
				{ "char", "<slot|name|#id> [pic]",
					"pick the character of the character screen", command_char },
				{ "cancel", "",
					"stop the login script and its prompts", command_cancel },
			});
		}

		void tick()
		{
			if (!script.running)
				return;

			// A prompt which 'cancel' dropped instead of answering stops the script,
			// otherwise it would wait for a line nobody is going to hand it
			if (script.asked && !debug_console::is_awaiting())
			{
				stop("the prompt was cancelled");
				return;
			}

			if (script.asked)
				return;

			Screen screen = observe();

			if (screen != script.screen)
			{
				script.screen = screen;
				script.acted = false;
				script.changed = now();
			}

			if (now() - script.changed > TIMEOUT)
			{
				stop("nothing happened for " + std::to_string(TIMEOUT / 1000) + " seconds");
				return;
			}

			if (script.acted)
				return;

			if (screen == Screen::Game)
			{
				std::cout << "login script: the character is in the game" << std::endl;

				script.running = false;
				return;
			}

			act(screen);
		}

		void cancel()
		{
			if (!script.running && !script.asked)
			{
				std::cout << "no login script is running" << std::endl;
				return;
			}

			stop("cancelled");
		}
	}
}
