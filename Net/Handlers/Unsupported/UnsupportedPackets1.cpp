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
		// 1 GUEST_ID_LOGIN, 6 CHECK_PINCODE, 7 UPDATE_PINCODE, 8 VIEW_ALL_CHAR, 22 RELOG_RESPONSE, 26 LAST_CONNECTED_WORLD, 27 RECOMMENDED_WORLD_MESSAGE, 30 INVENTORY_GROW, 34 FORCED_STAT_SET, 38 FAME_RESPONSE, 41 MEMO_RESULT, 42 MAP_TRANSFER_RESULT, 43 WEDDING_PHOTO, 47 CLAIM_STATUS_CHANGED, 48 SET_TAMING_MOB_INFO, 49 QUEST_CLEAR, 50 ENTRUSTED_SHOP_CHECK_RESULT, 51 SKILL_LEARN_ITEM_RESULT, 55 SUE_CHARACTER_RESULT, 57 TRADE_MONEY_LIMIT, 58 SET_GENDER, 59 GUILD_BBS_PACKET, 62 PARTY_OPERATION, 63 BUDDYLIST
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.

		namespace
		{
			// GameConstants.hasSPTable (GameConstants.java:604) is true for the Evan
			// jobs only; those send one skill book entry per advancement instead of a
			// single remaining sp value. The ids are the ones of Job.java:63-66.
			bool has_sp_table(int16_t job)
			{
				switch (job)
				{
				case 2001: // EVAN
				case 2200: // EVAN1
				case 2210: // EVAN2
				case 2211: // EVAN3
				case 2212: // EVAN4
				case 2213: // EVAN5
				case 2214: // EVAN6
				case 2215: // EVAN7
				case 2216: // EVAN8
				case 2217: // EVAN9
				case 2218: // EVAN10
					return true;
				default:
					return false;
				}
			}

			// PacketCreator.addCharEquips (PacketCreator.java:298): the visible and the
			// masked equip slot/item list, each ending on a 0xFF slot, the cash weapon
			// and the item ids of the three pets.
			void parse_char_equips(InPacket& recv)
			{
				int8_t slot = recv.read_byte();

				while (slot != -1) // 0xFF ends the visible equips
				{
					recv.read_int(); // item id
					slot = recv.read_byte();
				}

				slot = recv.read_byte();

				while (slot != -1) // 0xFF ends the masked equips
				{
					recv.read_int(); // item id
					slot = recv.read_byte();
				}

				recv.read_int(); // cash weapon item id

				for (int i = 0; i < 3; i++)
					recv.read_int(); // pet item id
			}

			// PacketCreator.addCharLook (PacketCreator.java:215).
			void parse_char_look(InPacket& recv)
			{
				recv.read_byte(); // gender
				recv.read_byte(); // skin color
				recv.read_int();  // face
				recv.read_byte(); // writeBool(!mega)
				recv.read_int();  // hair

				parse_char_equips(recv);
			}

			// PacketCreator.addCharStats (PacketCreator.java:173).
			void parse_char_stats(InPacket& recv)
			{
				recv.read_int();             // character id
				recv.read_padded_string(13); // name
				recv.read_byte();            // gender
				recv.read_byte();            // skin color
				recv.read_int();             // face
				recv.read_int();             // hair

				for (int i = 0; i < 3; i++)
					recv.read_long(); // pet unique id

				recv.read_byte(); // level
				int16_t job = recv.read_short();

				for (int i = 0; i < 9; i++)
					recv.read_short(); // str, dex, int, luk, hp, maxhp, mp, maxmp, remaining ap

				if (has_sp_table(job))
				{
					// PacketCreator.addRemainingSkillInfo (PacketCreator.java:155)
					uint8_t books = static_cast<uint8_t>(recv.read_byte());

					for (uint8_t i = 0; i < books; i++)
					{
						recv.read_byte(); // skill book
						recv.read_byte(); // sp left in that book
					}
				}
				else
				{
					recv.read_short(); // remaining sp
				}

				recv.read_int();   // exp
				recv.read_short(); // fame
				recv.read_int();   // gacha exp
				recv.read_int();   // map id
				recv.read_byte();  // spawn point
				recv.read_int();   // 0, timestamp
			}

			// PacketCreator.addCharEntry (PacketCreator.java:336) as used by
			// showAllCharacterInfo: stats, look and the world rank block. The extra
			// byte of the character list variant is not written here.
			void parse_char_entry(InPacket& recv)
			{
				parse_char_stats(recv);
				parse_char_look(recv);

				// a GM character stops after the flag, everyone else gets the ranks
				if (recv.read_byte() != 0)
				{
					recv.read_int(); // world rank
					recv.read_int(); // world rank move
					recv.read_int(); // job rank
					recv.read_int(); // job rank move
				}
			}

			// PacketCreator.addPartyStatus (PacketCreator.java:3835): the member list is
			// padded to six entries, followed by the leader, the map ids and the doors.
			void parse_party_status(InPacket& recv)
			{
				for (int i = 0; i < 6; i++)
					recv.read_int(); // character id

				for (int i = 0; i < 6; i++)
					recv.read_padded_string(13); // name

				for (int i = 0; i < 6; i++)
					recv.read_int(); // job id

				for (int i = 0; i < 6; i++)
					recv.read_int(); // level

				for (int i = 0; i < 6; i++)
					recv.read_int(); // channel, -2 when offline

				recv.read_int(); // leader character id

				for (int i = 0; i < 6; i++)
					recv.read_int(); // map id

				for (int i = 0; i < 6; i++)
				{
					recv.read_int(); // door town id
					recv.read_int(); // door area id
					recv.read_int(); // door x
					recv.read_int(); // door y
				}
			}

			// GuildPackets.addThread (GuildPackets.java:228).
			void parse_bbs_thread(InPacket& recv)
			{
				recv.read_int();    // local thread id
				recv.read_int();    // poster character id
				recv.read_string(); // name
				recv.read_long();   // timestamp
				recv.read_int();    // icon
				recv.read_int();    // reply count
			}

			// server: PacketCreator.java:570 sendGuestTOS
			void parse_guest_id_login(InPacket& recv)
			{
				recv.read_short();  // 0x100
				recv.read_int();    // random value
				recv.read_long();   // 0
				recv.read_long();   // time of the last login
				recv.read_long();   // current server time
				recv.read_int();    // 0
				recv.read_string(); // url of the terms of service
			}

			// server: PacketCreator.java:741 pinOperation
			void parse_check_pincode(InPacket& recv)
			{
				recv.read_byte(); // 0 accepted, 1 register, 2 reenter, 4 enter pin
			}

			// server: PacketCreator.java:747 pinRegistered
			void parse_update_pincode(InPacket& recv)
			{
				recv.read_byte(); // 0
			}

			// server: PacketCreator.java:4619 showAllCharacterInfo
			//         PacketCreator.java:2510 showAllCharacter
			void parse_view_all_char(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				if (mode == 0)
				{
					// the character packets of the single worlds follow the count packet
					recv.read_byte(); // world id

					uint8_t count = static_cast<uint8_t>(recv.read_byte());

					for (uint8_t i = 0; i < count; i++)
						parse_char_entry(recv);

					recv.read_byte(); // 1 pic required, 2 pic not required
				}
				else
				{
					// 1 there are characters, 5 there are none
					recv.read_int(); // total worlds holding characters
					recv.read_int(); // total characters
				}
			}

			// server: PacketCreator.java:1196 getRelogResponse
			void parse_relog_response(InPacket& recv)
			{
				recv.read_byte(); // 1
			}

			// server: PacketCreator.java:2690 selectWorld
			void parse_last_connected_world(InPacket& recv)
			{
				recv.read_int(); // world id
			}

			// server: PacketCreator.java:2696 sendRecommended
			void parse_recommended_world_message(InPacket& recv)
			{
				uint8_t count = static_cast<uint8_t>(recv.read_byte());

				for (uint8_t i = 0; i < count; i++)
				{
					recv.read_int();    // world id
					recv.read_string(); // world name
				}
			}

			// server: PacketCreator.java:2438 updateInventorySlotLimit
			void parse_inventory_grow(InPacket& recv)
			{
				recv.read_byte(); // inventory type
				recv.read_byte(); // new slot limit
			}

			// server: PacketCreator.java:6065 aranGodlyStats
			void parse_forced_stat_set(InPacket& recv)
			{
				// twenty raw bytes, the values written by writeBytes
				for (int i = 0; i < 10; i++)
					recv.read_short();
			}

			// server: PacketCreator.java:3707 giveFameResponse
			//         PacketCreator.java:3728 giveFameErrorResponse
			//         PacketCreator.java:3734 receiveFame
			void parse_fame_response(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				if (mode == 0)
				{
					// the fame of another character was changed
					recv.read_string(); // name of that character
					recv.read_byte();   // 1 raised, 0 dropped
					recv.read_short();  // new fame
					recv.read_short();  // 0
				}
				else if (mode == 5)
				{
					// own fame was changed by another character
					recv.read_string(); // name of that character
					recv.read_byte();   // 1 raised, 0 dropped
				}

				// every other value is a giveFameErrorResponse status byte
			}

			// server: PacketCreator.java:5480 noteError
			void parse_memo_result(InPacket& recv)
			{
				recv.read_byte(); // 5
				recv.read_byte(); // 0 player online, 1 check the name, 2 inbox full
			}

			// server: PacketCreator.java:5499 trockRefreshMapList
			void parse_map_transfer_result(InPacket& recv)
			{
				recv.read_byte(); // 2 list cleared, 3 list refreshed

				if (recv.read_byte() != 0)
				{
					for (int i = 0; i < 10; i++)
						recv.read_int(); // vip teleport rock map ids
				}
				else
				{
					for (int i = 0; i < 5; i++)
						recv.read_int(); // teleport rock map ids
				}
			}

			// server: WeddingPackets.java:241 onTakePhoto
			void parse_wedding_photo(InPacket& recv)
			{
				recv.read_string(); // groom name
				recv.read_string(); // bride name
				recv.read_int();    // field id

				uint8_t count = static_cast<uint8_t>(recv.read_byte());

				for (uint8_t i = 0; i < count; i++)
				{
					parse_char_look(recv);       // guest avatar
					recv.read_int();             // 30000
					recv.read_int();             // 30000
					recv.read_string();          // guest name
					recv.read_string();          // guild name, empty without a guild
					recv.read_short();           // guild logo background
					recv.read_byte();            // guild logo background color
					recv.read_short();           // guild logo
					recv.read_byte();            // guild logo color
					recv.read_short();           // x
					recv.read_short();           // y
					recv.read_byte();            // 1
					recv.read_int();             // 1
					recv.read_string();          // guest name again
					recv.read_short();           // x
					recv.read_short();           // y
					recv.read_byte();            // stance
				}
			}

			// server: PacketCreator.java:5807 enableReport
			void parse_claim_status_changed(InPacket& recv)
			{
				recv.read_byte(); // 1
			}

			// server: PacketCreator.java:4631 updateMount
			void parse_set_taming_mob_info(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_int();  // mount level
				recv.read_int();  // mount exp
				recv.read_int();  // mount tiredness
				recv.read_byte(); // level up flag
			}

			// server: PacketCreator.java:3523 getShowQuestCompletion
			void parse_quest_clear(InPacket& recv)
			{
				recv.read_short(); // quest id
			}

			// server: PacketCreator.java:5057 hiredMerchantBox
			//         PacketCreator.java:5134 retrieveFirstMessage
			//         PacketCreator.java:5140 remoteChannelChange
			void parse_entrusted_shop_check_result(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				if (mode == 0x10)
				{
					// the store is open on another channel
					recv.read_int();  // 0, ignored by the server
					recv.read_byte(); // channel of the store
				}

				// 0x07 and 0x09 carry the mode byte only
			}

			// server: PacketCreator.java:4586 skillBookResult
			void parse_skill_learn_item_result(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_byte(); // 1
				recv.read_int();  // skill id
				recv.read_int();  // max level of the skill
				recv.read_byte(); // 1 when the job allows the skill
				recv.read_byte(); // 1 when the skill was learned
			}

			// server: PacketCreator.java:6171 reportResponse
			void parse_sue_character_result(InPacket& recv)
			{
				recv.read_byte(); // report result
			}

			// server: PacketCreator.java:6520 sendMesoLimit
			// The server sends the opcode without any payload.
			void parse_trade_money_limit(InPacket& recv)
			{
				(void)recv;
			}

			// server: PacketCreator.java:5801 updateGender
			void parse_set_gender(InPacket& recv)
			{
				recv.read_byte(); // gender
			}

			// server: GuildPackets.java:235 BBSThreadList
			//         GuildPackets.java:265 showThread
			void parse_guild_bbs_packet(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				if (mode == 0x06)
				{
					// thread list of a guild, a notice thread can precede the listed ones
					if (recv.read_byte() != 0)
						parse_bbs_thread(recv);

					recv.read_int(); // total threads
					int32_t listed = recv.read_int();

					for (int32_t i = 0; i < listed; i++)
						parse_bbs_thread(recv);
				}
				else if (mode == 0x07)
				{
					// one thread with all of its replies
					recv.read_int();    // local thread id
					recv.read_int();    // poster character id
					recv.read_long();   // timestamp
					recv.read_string(); // name
					recv.read_string(); // starting post
					recv.read_int();    // icon

					int32_t replies = recv.read_int();

					for (int32_t i = 0; i < replies; i++)
					{
						recv.read_int();    // reply id
						recv.read_int();    // poster character id
						recv.read_long();   // timestamp
						recv.read_string(); // content
					}
				}
			}

			// server: PacketCreator.java:3742 partyCreated
			//         PacketCreator.java:3772 partyInvite
			//         PacketCreator.java:3781 partySearchInvite
			//         PacketCreator.java:3803 partyStatusMessage
			//         PacketCreator.java:3817 partyStatusMessage
			//         PacketCreator.java:3889 updateParty
			//         PacketCreator.java:3933 partyPortal
			void parse_party_operation(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				switch (mode)
				{
				case 4: // partyInvite and partySearchInvite
					recv.read_int();    // party id
					recv.read_string(); // inviter name
					recv.read_byte();   // 0
					break;
				case 8: // partyCreated
					recv.read_int(); // party id
					recv.read_int(); // door town id
					recv.read_int(); // door area id
					recv.read_int(); // door x
					recv.read_int(); // door y
					break;
				case 0x07: // updateParty SILENT_UPDATE and LOG_ONOFF
					recv.read_int(); // party id
					parse_party_status(recv);
					break;
				case 0x0C: // updateParty DISBAND, EXPEL and LEAVE
					recv.read_int(); // party id
					recv.read_int(); // target character id

					if (recv.read_byte() == 0)
					{
						recv.read_int(); // party id, repeated for a disband
					}
					else
					{
						recv.read_byte();   // 1 when the target was expelled
						recv.read_string(); // target name
						parse_party_status(recv);
					}
					break;
				case 0x0F: // updateParty JOIN
					recv.read_int();    // party id
					recv.read_string(); // target name
					parse_party_status(recv);
					break;
				case 0x1B: // updateParty CHANGE_LEADER
					recv.read_int();  // new leader character id
					recv.read_byte(); // 0
					break;
				case 0x23: // partyPortal, written as a short
					recv.skip(1);      // high byte of writeShort(0x23)
					recv.read_int();   // town id
					recv.read_int();   // target character id
					recv.skip_point(); // position
					break;
				default:
					// partyStatusMessage: only the overload taking a name writes one,
					// the wire carries no flag telling the two apart
					if (recv.available())
						recv.read_string();
					break;
				}
			}

			// server: PacketCreator.java:4096 updateBuddylist
			//         PacketCreator.java:4116 buddylistMessage
			//         PacketCreator.java:4122 requestBuddylistAdd
			//         PacketCreator.java:4139 updateBuddyChannel
			//         PacketCreator.java:4155 updateBuddyCapacity
			void parse_buddylist(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				switch (mode)
				{
				case 7: // updateBuddylist
				{
					uint8_t count = static_cast<uint8_t>(recv.read_byte());

					// only visible buddies get an entry of 39 bytes, while the trailing
					// map id block holds one int per buddy, so entries are read while
					// more than that block is left
					while (recv.length() > static_cast<size_t>(count) * 4)
					{
						recv.read_int();             // character id
						recv.read_padded_string(13); // name
						recv.read_byte();            // opposite status
						recv.read_int();             // channel
						recv.read_padded_string(13); // group
						recv.read_int();             // map id
					}

					for (uint8_t i = 0; i < count; i++)
						recv.read_int(); // map id of a buddy without an entry
					break;
				}
				case 9: // requestBuddylistAdd
					recv.read_int();             // inviter character id
					recv.read_string();          // inviter name
					recv.read_int();             // inviter character id
					recv.read_padded_string(13); // inviter name
					recv.read_byte();            // 0x09
					recv.read_byte();            // 0xf0
					recv.read_byte();            // 0x01
					recv.read_int();             // 0x0f
					recv.read_padded_string(13); // default group
					recv.read_byte();            // 0
					recv.read_int();             // character id of the buddy
					break;
				case 0x14: // updateBuddyChannel
					recv.read_int();  // character id
					recv.read_byte(); // 0
					recv.read_int();  // channel
					break;
				case 0x15: // updateBuddyCapacity
					recv.read_byte(); // capacity
					break;
				default: // buddylistMessage: the message byte is the whole payload
					break;
				}
			}
		}

		void register_packets1()
		{
			add(ServerOpcode::GUEST_ID_LOGIN, parse_guest_id_login);
			add(ServerOpcode::CHECK_PINCODE, parse_check_pincode);
			add(ServerOpcode::UPDATE_PINCODE, parse_update_pincode);
			add(ServerOpcode::VIEW_ALL_CHAR, parse_view_all_char);
			add(ServerOpcode::RELOG_RESPONSE, parse_relog_response);
			add(ServerOpcode::LAST_CONNECTED_WORLD, parse_last_connected_world);
			add(ServerOpcode::RECOMMENDED_WORLD_MESSAGE, parse_recommended_world_message);
			add(ServerOpcode::INVENTORY_GROW, parse_inventory_grow);
			add(ServerOpcode::FORCED_STAT_SET, parse_forced_stat_set);
			add(ServerOpcode::FAME_RESPONSE, parse_fame_response);
			add(ServerOpcode::MEMO_RESULT, parse_memo_result);
			add(ServerOpcode::MAP_TRANSFER_RESULT, parse_map_transfer_result);
			add(ServerOpcode::WEDDING_PHOTO, parse_wedding_photo);
			add(ServerOpcode::CLAIM_STATUS_CHANGED, parse_claim_status_changed);
			add(ServerOpcode::SET_TAMING_MOB_INFO, parse_set_taming_mob_info);
			add(ServerOpcode::QUEST_CLEAR, parse_quest_clear);
			add(ServerOpcode::ENTRUSTED_SHOP_CHECK_RESULT, parse_entrusted_shop_check_result);
			add(ServerOpcode::SKILL_LEARN_ITEM_RESULT, parse_skill_learn_item_result);
			add(ServerOpcode::SUE_CHARACTER_RESULT, parse_sue_character_result);
			add(ServerOpcode::TRADE_MONEY_LIMIT, parse_trade_money_limit);
			add(ServerOpcode::SET_GENDER, parse_set_gender);
			add(ServerOpcode::GUILD_BBS_PACKET, parse_guild_bbs_packet);
			add(ServerOpcode::PARTY_OPERATION, parse_party_operation);
			add(ServerOpcode::BUDDYLIST, parse_buddylist);
		}
	}
}