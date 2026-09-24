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
#include "LoginHandlers.h"

#include "Helpers/LoginParser.h"

#include "../Packets/LoginPackets.h"

#include "../../IO/UI.h"

#include "../../IO/UITypes/UICharSelect.h"
#include "../../IO/UITypes/UIGender.h"
#include "../../IO/UITypes/UILoginNotice.h"
#include "../../IO/UITypes/UILoginWait.h"
#include "../../IO/UITypes/UIRaceSelect.h"
#include "../../IO/UITypes/UITermsOfService.h"
#include "../../IO/UITypes/UIWorldSelect.h"

namespace ms
{
	void LoginResultHandler::handle(InPacket& recv) const
	{
		auto loginwait = UI::get().get_element<UILoginWait>();

		if (loginwait && loginwait->is_active())
		{
			// Remove previous UIs
			UI::get().remove(UIElement::Type::LOGINNOTICE);
			UI::get().remove(UIElement::Type::LOGINWAIT);
			UI::get().remove(UIElement::Type::TOS);
			UI::get().remove(UIElement::Type::GENDER);

			std::function<void()> okhandler = loginwait->get_handler();

			// The packet should contain a 'reason' byte which can signify various
			// things (PacketCreator.java:629-634 writes it as a single byte); the
			// 32-bit read below covers the rest of the empty status packet.
			if (int32_t reason = recv.read_int())
			{
				// Login unsuccessful
				// The LoginNotice displayed will contain the specific information
				switch (reason)
				{
					case 2: // account banned or temp banned, PacketCreator.java:675-691
					case 3: // banned IP or MAC, LoginPasswordHandler.java:98-101
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::BLOCKED_ID, okhandler);
						break;
					}
					case 4: // incorrect password, Client.java:700-702
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::WRONG_PASSWORD, okhandler);
						break;
					}
					case 5: // id is not registered, Client.java:653-654
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NOT_REGISTERED, okhandler);
						break;
					}
					case 6: // too many login attempts, Client.java:656-660
					case 10: // a login is already being processed, Client.java:724
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::TOO_MANY_REQUESTS, okhandler);
						break;
					}
					case 7: // already logged in, Client.java:691-692, LoginPasswordHandler.java:116-120
					case 17: // session is logged in elsewhere, Client.java:722
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::ALREADY_LOGGED_IN, okhandler);
						break;
					}
					case 8: // multi-client check failed, Client.java:719-726
					case 9: // invalid terms of service accept, AcceptToSHandler.java:26-27
					case 16: // too many accounts used from this host, Client.java:725
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::TROUBLE_LOGGING_IN, okhandler);
						break;
					}
					case 13: // multi-client limit reached, Client.java:723
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::UNABLE_TO_LOGIN_WITH_IP, okhandler);
						break;
					}
					case 14: // remote address could not be resolved, LoginPasswordHandler.java:50-51
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::WRONG_GATEWAY, okhandler);
						break;
					}
					case 15: // account id from the database is invalid, Client.java:671-673
					{
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::CANNOT_ACCESS_ACCOUNT, okhandler);
						break;
					}
					case 23: // terms of service were not accepted, Client.java:695-696
					{
						UI::get().emplace<UITermsOfService>(okhandler);
						break;
					}
					default:
					{
						// Reason 1 aborts a pending request (CreateCharHandler.java:120-121);
						// all other reasons have no notice of their own
						UI::get().emplace<UILoginNotice>(UILoginNotice::Message::UNKNOWN_ERROR, okhandler);
						break;
					}
				}
			}
			else
			{
				// Login successful
				// The packet contains information on the account, so we initialize the account with it.
				Account account = LoginParser::parse_account(recv);

				Configuration::get().set_admin(account.admin);

				if (account.female == 10)
				{
					UI::get().emplace<UIGender>(okhandler);
				}
				else
				{
					// Save the "Login ID" if the box for it on the login screen is checked
					if (Setting<SaveLogin>::get().load())
						Setting<DefaultAccount>::get().save(account.name);

					// Request the list of worlds and channels online
					ServerRequestPacket().dispatch();
				}
			}
		}
	}

	void ServerStatusHandler::handle(InPacket& recv) const
	{
		// Possible values for status (PacketCreator.java:823-833, World.java:639-656):
		// 0 - Normal
		// 1 - Highly populated (at least 80% of the world capacity is in use)
		// 2 - Full
		int16_t status = recv.read_short();

		if (status == 0)
			return;

		// This client has no UI for the world status yet
		LOG(LOG_NETWORK, "[ServerStatusHandler] World status: [{}]", status);

		if (status == 2)
		{
			// A full world or an unknown channel answers a character list request with
			// this packet instead of CHARLIST (CharlistRequestHandler.java:38-50), so the
			// 'entering world' wait has to be dropped to return to the world selection.
			UI::get().remove(UIElement::Type::LOGINWAIT);
		}
	}

	void SelectCharacterHandler::handle(InPacket& recv) const
	{
		std::function<void()> okhandler = []() {};

		// The packet should contain a 'reason' short which can signify various
		// things (PacketCreator.java:657-661). Reasons 7, 8, 9, 10 and 17 are the
		// ones this server sends (CharSelectedHandler.java:43-49,64,91,97 and the
		// other *CharSelected* handlers); those numbers do not carry the meaning
		// the LOGIN_STATUS table at PacketCreator.java:636-656 gives them.
		if (int16_t reason = recv.read_short())
		{
			// Select character unsuccessful
			// The LoginNotice displayed will contain the specific information
			switch (reason)
			{
				case 2: // id deleted or blocked, PacketCreator.java:636-656, not sent here
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::BLOCKED_ID, okhandler);
					break;
				}
				case 5: // id is not registered, PacketCreator.java:636-656, not sent here
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NOT_REGISTERED, okhandler);
					break;
				}
				case 7: // another session is logged in, CharSelectedHandler.java:45
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::ALREADY_LOGGED_IN, okhandler);
					break;
				}
				case 8: // coordinator error, CharSelectedHandler.java:47
				case 9: // no session matched, CharSelectedHandler.java:48
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::TROUBLE_LOGGING_IN, okhandler);
					break;
				}
				case 10: // another session is being processed / world is full, CharSelectedHandler.java:44,91,97
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::TOO_MANY_REQUESTS, okhandler);
					break;
				}
				case 13: // unable to log on as master at this ip, PacketCreator.java:636-656, not sent here
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::UNABLE_TO_LOGIN_WITH_IP, okhandler);
					break;
				}
				case 17: // host or hwid does not match, CharSelectedHandler.java:46,64
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::WRONG_GATEWAY, okhandler);
					break;
				}
				case 23: // crashes, PacketCreator.java:636-656, not sent here
				{
					UI::get().emplace<UITermsOfService>(okhandler);
					break;
				}
				default:
				{
					UI::get().emplace<UILoginNotice>(UILoginNotice::Message::UNKNOWN_ERROR, okhandler);
					break;
				}
			}
		}
	}

	void ServerlistHandler::handle(InPacket& recv) const
	{
		auto worldselect = UI::get().get_element<UIWorldSelect>();

		if (!worldselect)
			worldselect = UI::get().emplace<UIWorldSelect>();

		// Parse all worlds
		while (recv.available())
		{
			World world = LoginParser::parse_world(recv);

			if (world.id != -1)
			{
				worldselect->add_world(world);
			}
			else
			{
				// Remove previous UIs
				UI::get().remove(UIElement::Type::LOGIN);

				// Add the world selection screen to the UI
				worldselect->draw_world();

				// End of packet
				return;
			}
		}
	}

	void CharlistHandler::handle(InPacket& recv) const
	{
		auto loginwait = UI::get().get_element<UILoginWait>();

		if (loginwait && loginwait->is_active())
		{
			uint8_t channel_id = recv.read_byte();

			// Parse all characters
			std::vector<CharEntry> characters;
			int8_t charcount = recv.read_byte();

			for (uint8_t i = 0; i < charcount; ++i)
				characters.emplace_back(LoginParser::parse_charentry(recv));

			int8_t pic = recv.read_byte();
			int32_t slots = recv.read_int();

			// Remove previous UIs
			UI::get().remove(UIElement::Type::LOGINNOTICE);
			UI::get().remove(UIElement::Type::LOGINWAIT);

			// Remove the world selection screen
			if (auto worldselect = UI::get().get_element<UIWorldSelect>())
				worldselect->remove_selected();

			// Add the character selection screen
			UI::get().emplace<UICharSelect>(characters, charcount, slots, pic);
		}
	}

	void ServerIPHandler::handle(InPacket& recv) const
	{
		recv.skip_byte();

		LoginParser::parse_login(recv);

		int32_t cid = recv.read_int();
		PlayerLoginPacket(cid).dispatch();
	}

	void CharnameResponseHandler::handle(InPacket& recv) const
	{
		// Read the name and if it is already in use
		std::string name = recv.read_string();
		bool used = recv.read_bool();

		// Notify the character creation screen
		if (auto raceselect = UI::get().get_element<UIRaceSelect>())
			raceselect->send_naming_result(used);
	}

	void AddNewCharEntryHandler::handle(InPacket& recv) const
	{
		recv.skip(1);

		// Parse info on the new character
		CharEntry character = LoginParser::parse_charentry(recv);

		// Read the updated character selection
		if (auto charselect = UI::get().get_element<UICharSelect>())
			charselect->add_character(std::move(character));
	}

	void DeleteCharResponseHandler::handle(InPacket& recv) const
	{
		// Read the character id and if deletion was successful (PIC was correct)
		int32_t cid = recv.read_int();
		uint8_t state = recv.read_byte();

		// Extract information from the state byte
		// The states the server can send are documented at PacketCreator.java:2667-2682:
		// 0x00 success, 0x06 trouble logging in, 0x09 unknown error, 0x0A too many
		// connection requests, 0x12 invalid birthday, 0x14 incorrect pic, 0x16 guild
		// master, 0x18 pending wedding, 0x1A pending world transfer, 0x1D has a family.
		// DeleteCharHandler.java:62,67,77,83,88,90,93 sends 0x16, 0x1D, 0x1A, 0x09,
		// 0x00, 0x09 and 0x14, CreateCharHandler.java:57-123 sends 0x09 when the
		// creation request contains illegal parameters.
		if (state)
		{
			UILoginNotice::Message message;

			switch (state)
			{
				case 0x06:
					message = UILoginNotice::Message::TROUBLE_LOGGING_IN;
					break;
				case 0x0A:
					message = UILoginNotice::Message::TOO_MANY_REQUESTS;
					break;
				case 0x12:
					message = UILoginNotice::Message::BIRTHDAY_INCORRECT;
					break;
				case 0x14:
					message = UILoginNotice::Message::INCORRECT_PIC;
					break;
				case 0x16:
					message = UILoginNotice::Message::CANNOT_DELETE_GUILD_LEADER;
					break;
				case 0x18:
					message = UILoginNotice::Message::CANNOT_DELETE_ENGAGED;
					break;
				case 0x1A:
					// The client has no message of its own for a pending world
					// transfer, 106 follows CHAR_TRANS_SUCCESS (105) in UILoginNotice.h
					message = UILoginNotice::Message::CHAR_DEL_FAIL_MAX_LIMIT_REACHED;
					break;
				case 0x1D:
					message = UILoginNotice::Message::CANNOT_DELETE_FAMILY_LEADER;
					break;
				default:
					message = UILoginNotice::Message::UNKNOWN_ERROR;
			}

			UI::get().emplace<UILoginNotice>(message);
		}
		else
		{
			if (auto charselect = UI::get().get_element<UICharSelect>())
				charselect->remove_character(cid);
		}
	}
}