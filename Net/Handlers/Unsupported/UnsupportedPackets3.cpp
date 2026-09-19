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
#include "../../InPacket.h"
#include "../../ServerOpcodes.h"
#include "../../UnsupportedPackets.h"

namespace ms
{
	namespace Unsupported
	{
		// Opcodes handled by this table (server is authoritative for every layout):
		// 102 FAMILY_NOTIFY_LOGIN_OR_LOGOUT, 103 FAMILY_SET_PRIVILEGE, 104 FAMILY_SUMMON_REQUEST, 105 NOTIFY_LEVELUP, 106 NOTIFY_MARRIAGE, 107 NOTIFY_JOB_CHANGE, 111 SET_AVATAR_MEGAPHONE, 112 CLEAR_AVATAR_MEGAPHONE, 113 CANCEL_NAME_CHANGE_RESULT, 114 CANCEL_TRANSFER_WORLD_RESULT, 116 FAKE_GM_NOTICE, 117 SUCCESS_IN_USE_GACHAPON_BOX, 118 NEW_YEAR_CARD_RES, 121 SET_EXTRA_PENDANT_SLOT, 122 SCRIPT_PROGRESS_MESSAGE, 123 DATA_CRC_CHECK_FAILED, 128 SET_BACK_EFFECT, 131 BLOCKED_MAP, 132 BLOCKED_SERVER, 133 FORCED_MAP_EQUIP, 134 MULTICHAT, 135 WHISPER, 136 SPOUSE_CHAT, 139 FIELD_OBSTACLE_ONOFF
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		// Unsupported::forward() logs the parsed byte count and states that the
		// capability itself is not implemented.
		namespace
		{
			// The WHISPER flag bits the server sends (PacketCreator.WhisperFlag);
			// LOCATION (0x01) and LOCATION_FRIEND (0x40) are only carried in the
			// byte together with RESULT, so they need no branch of their own.
			const int8_t WHISPER_FLAG_WHISPER = 0x02;
			const int8_t WHISPER_FLAG_RESULT = 0x08;
			const int8_t WHISPER_FLAG_RECEIVE = 0x10;

			// Mirrors PacketCreator.sendFamilyLoginNotice
			void parse_family_notify_login_or_logout(InPacket& recv)
			{
				recv.read_bool();    // logged in (false: logged out)
				recv.read_string();  // character name
			}

			// Mirrors PacketCreator.familyBuff
			void parse_family_set_privilege(InPacket& recv)
			{
				int8_t type = recv.read_byte();

				// The buff block is only written for the three granted privilege
				// types (0 cancels a buff and carries no further fields)
				if (type >= 2 && type <= 4)
				{
					recv.read_int();   // buff number
					recv.read_int();   // amount (0 when type == 3)
					recv.read_int();   // amount (0 when type == 2)
					recv.read_byte();  // always written as 0
					recv.read_int();   // remaining time
				}
			}

			// Mirrors PacketCreator.sendFamilySummonRequest
			void parse_family_summon_request(InPacket& recv)
			{
				recv.read_string();  // name of the family member summoning
				recv.read_string();  // family name
			}

			// Mirrors PacketCreator.levelUpMessage
			void parse_notify_levelup(InPacket& recv)
			{
				recv.read_byte();    // type: 0 family reset, 1 family, 2 guild
				recv.read_int();     // reached level
				recv.read_string();  // character name
			}

			// Mirrors PacketCreator.marriageMessage
			void parse_notify_marriage(InPacket& recv)
			{
				recv.read_byte();    // type: 0 guild, 1 family
				recv.read_string();  // "> " + character name
			}

			// Mirrors PacketCreator.jobMessage
			void parse_notify_job_change(InPacket& recv)
			{
				recv.read_byte();    // type: 0 guild, 1 family
				recv.read_int();     // job id
				recv.read_string();  // "> " + character name
			}

			// Mirrors PacketCreator.encodeNewYearCard
			void parse_new_year_card(InPacket& recv)
			{
				recv.read_int();     // card id
				recv.read_int();     // sender character id
				recv.read_string();  // sender name
				recv.read_bool();    // sender discarded the card
				recv.read_long();    // date sent
				recv.read_int();     // receiver character id
				recv.read_string();  // receiver name
				recv.read_bool();    // receiver discarded the card
				recv.read_bool();    // receiver received the card
				recv.read_long();    // date received
				recv.read_string();  // card message
			}

			// Mirrors PacketCreator.getAvatarMega.
			// The message line list is written without a count, so its length is not
			// on the wire. Every emit site of this server build passes exactly four
			// lines (UseCashItemHandler cash item 539 reads four strings from the
			// client, Character.showMapOwnershipInfo builds four), which is what this
			// parser assumes.
			void parse_set_avatar_megaphone(InPacket& recv)
			{
				recv.read_int();     // cash item id
				recv.read_string();  // "<medal name><character name>"

				for (int i = 0; i < 4; i++)
					recv.read_string();  // message line

				recv.read_int();   // channel - 1
				recv.read_bool();  // ear shown

				// Mirrors PacketCreator.addCharLook(chr, mega = true)
				recv.read_byte();  // gender
				recv.read_byte();  // skin colour
				recv.read_int();   // face
				recv.read_bool();  // !mega, false for the megaphone look
				recv.read_int();   // hair

				// Mirrors PacketCreator.addCharEquips: both equip maps are written
				// as (position byte, item id int) pairs terminated by a 0xFF byte
				while (static_cast<uint8_t>(recv.read_byte()) != 0xFF)
					recv.read_int();  // visible equip

				while (static_cast<uint8_t>(recv.read_byte()) != 0xFF)
					recv.read_int();  // masked equip

				recv.read_int();  // cash weapon
				recv.read_int();  // pet 1 item id
				recv.read_int();  // pet 2 item id
				recv.read_int();  // pet 3 item id
			}

			// Mirrors PacketCreator.byeAvatarMega
			void parse_clear_avatar_megaphone(InPacket& recv)
			{
				recv.read_byte();  // always written as 1
			}

			// Mirrors PacketCreator.showNameChangeCancel
			void parse_cancel_name_change_result(InPacket& recv)
			{
				bool success = recv.read_bool();

				// The server writes a reason byte only on failure
				if (!success)
					recv.read_byte();  // reason (the message is never written)
			}

			// Mirrors PacketCreator.showWorldTransferCancel
			void parse_cancel_transfer_world_result(InPacket& recv)
			{
				bool success = recv.read_bool();

				// The server writes a reason byte only on failure
				if (!success)
					recv.read_byte();  // reason (the message is never written)
			}

			// Mirrors PacketCreator.sendPolice
			void parse_fake_gm_notice(InPacket& recv)
			{
				recv.read_byte();  // value carries no meaning
			}

			// Mirrors PacketCreator.UseTreasureBox
			void parse_success_in_use_gachapon_box(InPacket& recv)
			{
				recv.read_int();  // type
			}

			// Mirrors PacketCreator.onNewYearCardRes
			void parse_new_year_card_res(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				switch (mode)
				{
				case 4:  // successfully sent a card
				case 6:  // successfully received a card
					parse_new_year_card(recv);
					break;
				case 8:  // successfully deleted a card
					recv.read_int();  // card id
					break;
				case 5:  // error replies, all sharing the "reason code" layout
				case 7:
				case 9:
				case 0xB:
					recv.read_byte();  // reason code
					break;
				case 0xA:  // unreceived card list
				{
					int32_t count = recv.read_int();

					// The server only writes the list for 1 <= count <= 99
					if (count > 0 && count <= 99)
					{
						for (int32_t i = 0; i < count; i++)
						{
							recv.read_int();     // card id
							recv.read_int();     // sender character id
							recv.read_string();  // sender name
						}
					}
					break;
				}
				case 0xC:  // card arrived
					recv.read_int();     // card id
					recv.read_string();  // sender name
					break;
				case 0xD:  // broadcast: add the card to the list
					recv.read_int();  // card id
					recv.read_int();  // receiving character id
					break;
				case 0xE:  // broadcast: remove the card from the list
					recv.read_int();  // card id
					break;
				default:  // modes without a payload after the mode byte
					break;
				}
			}

			// Mirrors PacketCreator.setExtraPendantSlot
			void parse_set_extra_pendant_slot(InPacket& recv)
			{
				recv.read_bool();  // extra pendant slot enabled
			}

			// Mirrors PacketCreator.earnTitleMessage
			void parse_script_progress_message(InPacket& recv)
			{
				recv.read_string();  // message
			}

			// Mirrors PacketCreator.sendPolice(String)
			void parse_data_crc_check_failed(InPacket& recv)
			{
				recv.read_string();  // message
			}

			// Mirrors PacketCreator.changeBackgroundEffect
			void parse_set_back_effect(InPacket& recv)
			{
				recv.read_bool();  // remove the layer instead of adding it
				recv.read_int();   // always written as 0
				recv.read_byte();  // layer
				recv.read_int();   // transition time
			}

			// Mirrors PacketCreator.blockedMessage
			void parse_blocked_map(InPacket& recv)
			{
				recv.read_byte();  // reason type
			}

			// Mirrors PacketCreator.blockedMessage2
			void parse_blocked_server(InPacket& recv)
			{
				recv.read_byte();  // reason type
			}

			// Mirrors PacketCreator.showForcedEquip
			void parse_forced_map_equip(InPacket& recv)
			{
				// The team byte is omitted when the server is called with team < 0
				if (recv.length() > 0)
					recv.read_byte();  // 0 = red, 1 = blue
			}

			// Mirrors PacketCreator.multiChat
			void parse_multichat(InPacket& recv)
			{
				recv.read_byte();    // mode: 0 buddy, 1 party, 2 guild
				recv.read_string();  // speaker name
				recv.read_string();  // chat text
			}

			// Mirrors PacketCreator.getFindResult, getWhisperResult and
			// getWhisperReceive, the only three packets the server sends on WHISPER
			void parse_whisper(InPacket& recv)
			{
				int8_t flags = recv.read_byte();

				if ((flags & WHISPER_FLAG_RESULT) != 0)
				{
					if ((flags & WHISPER_FLAG_WHISPER) != 0)
					{
						// WHISPER | RESULT: getWhisperResult
						recv.read_string();  // target name
						recv.read_bool();    // message delivered
					}
					else
					{
						// LOCATION | RESULT and LOCATION_FRIEND | RESULT:
						// getFindResult, both with the same layout
						recv.read_string();  // target name
						int8_t type = recv.read_byte();  // WhisperHandler location type
						recv.read_int();     // map id or channel - 1

						// RT_SAME_CHANNEL carries the target position as well
						if (type == 1)
						{
							recv.read_int();  // position x
							recv.read_int();  // position y
						}
					}
				}
				else if ((flags & WHISPER_FLAG_RECEIVE) != 0)
				{
					// WHISPER | RECEIVE: getWhisperReceive
					recv.read_string();  // sender name
					recv.read_byte();    // sender channel - 1
					recv.read_bool();    // sent by an admin
					recv.read_string();  // message
				}
			}

			// Mirrors PacketCreator.OnCoupleMessage
			void parse_spouse_chat(InPacket& recv)
			{
				int8_t spouse = recv.read_byte();

				// The sender name is only written when the packet came from the spouse
				if (spouse == 5)
					recv.read_string();  // spouse name

				recv.read_byte();    // 5 from the spouse, 1 from the fiance
				recv.read_string();  // chat text
			}

			// Mirrors PacketCreator.environmentMove
			void parse_field_obstacle_onoff(InPacket& recv)
			{
				recv.read_string();  // obstacle name
				recv.read_int();     // 0: stop and back to start, 1: move
			}
		}

		void register_packets3()
		{
			add(ServerOpcode::FAMILY_NOTIFY_LOGIN_OR_LOGOUT, parse_family_notify_login_or_logout);
			add(ServerOpcode::FAMILY_SET_PRIVILEGE, parse_family_set_privilege);
			add(ServerOpcode::FAMILY_SUMMON_REQUEST, parse_family_summon_request);
			add(ServerOpcode::NOTIFY_LEVELUP, parse_notify_levelup);
			add(ServerOpcode::NOTIFY_MARRIAGE, parse_notify_marriage);
			add(ServerOpcode::NOTIFY_JOB_CHANGE, parse_notify_job_change);
			add(ServerOpcode::SET_AVATAR_MEGAPHONE, parse_set_avatar_megaphone);
			add(ServerOpcode::CLEAR_AVATAR_MEGAPHONE, parse_clear_avatar_megaphone);
			add(ServerOpcode::CANCEL_NAME_CHANGE_RESULT, parse_cancel_name_change_result);
			add(ServerOpcode::CANCEL_TRANSFER_WORLD_RESULT, parse_cancel_transfer_world_result);
			add(ServerOpcode::FAKE_GM_NOTICE, parse_fake_gm_notice);
			add(ServerOpcode::SUCCESS_IN_USE_GACHAPON_BOX, parse_success_in_use_gachapon_box);
			add(ServerOpcode::NEW_YEAR_CARD_RES, parse_new_year_card_res);
			add(ServerOpcode::SET_EXTRA_PENDANT_SLOT, parse_set_extra_pendant_slot);
			add(ServerOpcode::SCRIPT_PROGRESS_MESSAGE, parse_script_progress_message);
			add(ServerOpcode::DATA_CRC_CHECK_FAILED, parse_data_crc_check_failed);
			add(ServerOpcode::SET_BACK_EFFECT, parse_set_back_effect);
			add(ServerOpcode::BLOCKED_MAP, parse_blocked_map);
			add(ServerOpcode::BLOCKED_SERVER, parse_blocked_server);
			add(ServerOpcode::FORCED_MAP_EQUIP, parse_forced_map_equip);
			add(ServerOpcode::MULTICHAT, parse_multichat);
			add(ServerOpcode::WHISPER, parse_whisper);
			add(ServerOpcode::SPOUSE_CHAT, parse_spouse_chat);
			add(ServerOpcode::FIELD_OBSTACLE_ONOFF, parse_field_obstacle_onoff);
		}
	}
}