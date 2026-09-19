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
		// 276 REMOVE_DOOR, 281 SNOWBALL_STATE, 282 HIT_SNOWBALL, 283 SNOWBALL_MESSAGE, 284 LEFT_KNOCK_BACK, 285 COCONUT_HIT, 286 COCONUT_SCORE, 287 GUILD_BOSS_HEALER_MOVE, 288 GUILD_BOSS_PULLEY_STATE_CHANGE, 289 MONSTER_CARNIVAL_START, 290 MONSTER_CARNIVAL_OBTAINED_CP, 291 MONSTER_CARNIVAL_PARTY_CP, 292 MONSTER_CARNIVAL_SUMMON, 293 MONSTER_CARNIVAL_MESSAGE, 294 MONSTER_CARNIVAL_DIED, 297 ARIANT_ARENA_USER_SCORE, 299 SHEEP_RANCH_INFO, 300 SHEEP_RANCH_CLOTHES, 301 WITCH_TOWER_SCORE_UPDATE, 309 STORAGE, 310 FREDRICK_MESSAGE, 311 FREDRICK, 312 RPS_GAME, 313 MESSENGER
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		// The "capability not implemented" log line is written once per packet by
		// Unsupported::forward, together with the byte counts of the parser.

		namespace
		{
			// PacketCreator.addCharLook (PacketCreator.java:215) together with the item
			// lists of PacketCreator.addCharEquips (PacketCreator.java:290): gender,
			// skin colour, face, mega flag and hair, the equipped [slot][item id] pairs
			// up to their 0xFF terminator, the masked pairs up to the next terminator,
			// the cash weapon id and the item ids of the three pet slots
			void read_char_look(InPacket& recv)
			{
				recv.skip_byte(); // gender
				recv.skip_byte(); // skin colour
				recv.skip_int(); // face
				recv.skip_bool(); // mega, false for every packet of this table
				recv.skip_int(); // hair

				while (recv.read_byte() != -1)
					recv.skip_int(); // equipped item: slot byte and item id

				while (recv.read_byte() != -1)
					recv.skip_int(); // masked item: slot byte and item id

				recv.skip_int(); // cash weapon id, 0 when there is none
				recv.skip(12); // item ids of the three pet slots
			}

			// PacketCreator.addItemInfo (PacketCreator.java:388) on the zeroPosition
			// branch the storage and Fredrick packets use, which omits the position:
			// item type, item id, cash flag and unique id, expiration time, then the
			// layout of the item kind
			void read_item_info(InPacket& recv)
			{
				int8_t type = recv.read_byte(); // item type, 1 is an equip
				int32_t itemid = recv.read_int();
				bool cash = recv.read_bool();

				if (cash)
					recv.skip_long(); // unique id: pet id, ring id or cash id

				recv.skip_long(); // expiration time

				if (type == 1)
				{
					recv.skip(2); // upgrade slots and level
					recv.skip(30); // the fifteen equip stat shorts
					recv.skip_string(); // owner name
					recv.skip_short(); // item flags

					if (cash)
					{
						recv.skip(10); // ten 0x40 bytes
					}
					else
					{
						recv.skip(2); // 0 and item level
						recv.skip_int(); // exp nibble
						recv.skip_int(); // vicious hammer count
						recv.skip_long(); // 0
					}

					recv.skip_long(); // expiration time, always -2
					recv.skip_int(); // always -1
					return;
				}

				if (itemid / 1000 == 5000)
				{
					// A pet, ItemConstants.isPet (ItemConstants.java:109)
					recv.skip(13); // name, padded to a fixed thirteen bytes
					recv.skip_byte(); // level
					recv.skip_short(); // closeness
					recv.skip_byte(); // fullness
					recv.skip_long(); // expiration time
					recv.skip_short(); // pet attribute
					recv.skip_short(); // pet skill
					recv.skip_int(); // remaining life
					recv.skip_short(); // attribute
					return;
				}

				recv.skip_short(); // quantity
				recv.skip_string(); // owner name
				recv.skip_short(); // item flags

				// Throwing stars and bullets, ItemConstants.isRechargeable
				// (ItemConstants.java:93), add eight more bytes
				if (itemid / 10000 == 207 || itemid / 10000 == 233)
					recv.skip(8); // 2 and the four bytes 0x54 0x00 0x00 0x34
			}

			// A [byte count][item info] list, written by getStorage, storeStorage,
			// takeOutStorage, arrangeStorage and getFredrick(Character)
			void read_item_list(InPacket& recv)
			{
				int count = static_cast<uint8_t>(recv.read_byte());

				for (int i = 0; i < count; i++)
					read_item_info(recv);
			}
		}

		// PacketCreator.removeDoor (PacketCreator.java:1124) - the town branch of
		// removeDoor answers with SPAWN_PORTAL instead
		void parse_remove_door(InPacket& recv)
		{
			recv.read_byte(); // always 0
			recv.read_int(); // owner id of the door
		}

		// PacketCreator.rollSnowBall (PacketCreator.java:6852). The entermap branch
		// writes 21 blank bytes, the other branch the state of both snowballs, so the
		// remaining length tells the two apart
		void parse_snowball_state(InPacket& recv)
		{
			if (recv.length() == 21)
			{
				recv.skip(21);
				return;
			}

			recv.read_byte(); // 0 move, 1 roll, 2 the down snowball disappears, 3 the up one
			recv.read_int(); // hitpoints of the down snowball divided by 75
			recv.read_int(); // hitpoints of the up snowball divided by 75
			recv.read_short(); // position of the down snowball
			recv.read_byte(); // always -1
			recv.read_short(); // position of the up snowball
			recv.read_byte(); // always -1
		}

		// PacketCreator.hitSnowBall (PacketCreator.java:6868)
		void parse_hit_snowball(InPacket& recv)
		{
			recv.read_byte(); // what was hit
			recv.read_int(); // damage dealt
		}

		// PacketCreator.snowballMessage (PacketCreator.java:6886)
		void parse_snowball_message(InPacket& recv)
		{
			recv.read_byte(); // 0 the down team, 1 the up team
			recv.read_int(); // message at the top of the screen
		}

		// PacketCreator.leftKnockBack (PacketCreator.java:6848) - the packet carries no
		// payload, the knockback is fully determined by its opcode
		void parse_left_knock_back(InPacket&)
		{
		}

		// PacketCreator.hitCoconut (PacketCreator.java:6900). Both branches are a
		// short, a short and a byte: the spawn one sends the id -1 and the delay 5000
		void parse_coconut_hit(InPacket& recv)
		{
			recv.read_short(); // object id of the coconut, -1 to spawn one
			recv.read_short(); // delay until the next attack, 5000 on spawn
			recv.read_byte(); // animation of the coconut
		}

		// PacketCreator.coconutScore (PacketCreator.java:6893)
		void parse_coconut_score(InPacket& recv)
		{
			recv.read_short(); // score of the first team
			recv.read_short(); // score of the second team
		}

		// GuildBoss_HealerMove (GuildPackets.java:515)
		void parse_guild_boss_healer_move(InPacket& recv)
		{
			recv.read_short(); // new y position of the healer
		}

		// GuildBoss_PulleyStateChange (GuildPackets.java:521)
		void parse_guild_boss_pulley_state_change(InPacket& recv)
		{
			recv.read_byte(); // new state of the pulley
		}

		// PacketCreator.startMonsterCarnival (PacketCreator.java:7328)
		void parse_monster_carnival_start(InPacket& recv)
		{
			recv.read_byte(); // team of the player
			recv.read_short(); // obtained cp of the player
			recv.read_short(); // total cp of the player
			recv.read_short(); // obtained cp of the team
			recv.read_short(); // total cp of the team
			recv.read_short(); // obtained cp of the opposition
			recv.read_short(); // total cp of the opposition
			recv.read_short(); // always 0
			recv.read_long(); // always 0
		}

		// PacketCreator.CPUpdate (PacketCreator.java:7295) - the single player variant,
		// which is the one without a team byte
		void parse_monster_carnival_obtained_cp(InPacket& recv)
		{
			recv.read_short(); // obtained cp of the player
			recv.read_short(); // total cp of the player
		}

		// PacketCreator.CPUpdate (PacketCreator.java:7297) - the party variant
		void parse_monster_carnival_party_cp(InPacket& recv)
		{
			recv.read_byte(); // team
			recv.read_short(); // obtained cp of the team
			recv.read_short(); // total cp of the team
		}

		// PacketCreator.playerSummoned (PacketCreator.java:7312)
		void parse_monster_carnival_summon(InPacket& recv)
		{
			recv.read_byte(); // tab of the summon
			recv.read_byte(); // index within the tab
			recv.read_string(); // name of the summoned player
		}

		// PacketCreator.CPQMessage (PacketCreator.java:7306)
		void parse_monster_carnival_message(InPacket& recv)
		{
			recv.read_byte(); // message shown in the carnival window
		}

		// PacketCreator.playerDiedMessage (PacketCreator.java:7320)
		void parse_monster_carnival_died(InPacket& recv)
		{
			recv.read_byte(); // team of the player
			recv.read_string(); // name of the player
			recv.read_byte(); // cp the player lost
		}

		// PacketCreator.updateAriantPQRanking (PacketCreator.java:2529) - the count is
		// the number of entries, each of them a name and a score
		void parse_ariant_arena_user_score(InPacket& recv)
		{
			int count = static_cast<uint8_t>(recv.read_byte());

			for (int i = 0; i < count; i++)
			{
				recv.read_string(); // name of the player
				recv.read_int(); // score of the player
			}
		}

		// PacketCreator.sheepRanchInfo (PacketCreator.java:7342)
		void parse_sheep_ranch_info(InPacket& recv)
		{
			recv.read_byte(); // wolf id
			recv.read_byte(); // sheep id
		}

		// PacketCreator.sheepRanchClothes (PacketCreator.java:7350)
		void parse_sheep_ranch_clothes(InPacket& recv)
		{
			recv.read_int(); // character id
			recv.read_byte(); // 0 sheep, 1 wolf, 2 spectator
		}

		// PacketCreator.updateWitchTowerScore (PacketCreator.java:2539)
		void parse_witch_tower_score_update(InPacket& recv)
		{
			recv.read_byte(); // score of the player
		}

		// PacketCreator.getStorage (PacketCreator.java:3573), getStorageError
		// (PacketCreator.java:3597), mesoStorage (PacketCreator.java:3603),
		// storeStorage (PacketCreator.java:3614), takeOutStorage
		// (PacketCreator.java:3628) and arrangeStorage (PacketCreator.java:3642) - the
		// leading byte selects the variant
		void parse_storage(InPacket& recv)
		{
			int8_t mode = recv.read_byte();

			switch (mode)
			{
			case 0x16: // getStorage: the full storage window
				recv.read_int(); // npc id
				recv.read_byte(); // number of slots
				recv.read_short(); // always 0x7E
				recv.read_short(); // always 0
				recv.read_int(); // always 0
				recv.read_int(); // mesos kept in the storage
				recv.read_short(); // always 0
				read_item_list(recv); // items in the storage
				recv.read_short(); // always 0
				recv.read_byte(); // always 0
				break;
			case 0x13: // mesoStorage: mesos moved in or out
				recv.read_byte(); // number of slots
				recv.read_short(); // always 2
				recv.read_short(); // always 0
				recv.read_int(); // always 0
				recv.read_int(); // mesos kept in the storage
				break;
			case 0x0D: // storeStorage: items moved into the storage
			case 0x09: // takeOutStorage: items taken out of the storage
				recv.read_byte(); // number of slots
				recv.read_short(); // inventory type bitfield
				recv.read_short(); // always 0
				recv.read_int(); // always 0
				read_item_list(recv); // the items that moved
				break;
			case 0x0F: // arrangeStorage
				recv.read_byte(); // number of slots
				recv.read_byte(); // always 124
				recv.skip(10); // unused
				read_item_list(recv); // items in the storage
				recv.read_byte(); // always 0
				break;
			default: // getStorageError: the error code is the whole payload
				break;
			}
		}

		// PacketCreator.fredrickMessage (PacketCreator.java:4975)
		void parse_fredrick_message(InPacket& recv)
		{
			recv.read_byte(); // operation the client shows
		}

		// PacketCreator.getFredrick (PacketCreator.java:4981, 4997) - the first byte
		// selects the variant, 0x23 is the merchant window with its saved items
		void parse_fredrick(InPacket& recv)
		{
			int8_t op = recv.read_byte();

			switch (op)
			{
			case 0x23: // getFredrick(Character): the items the merchant still holds
				recv.read_int(); // fredrick npc id
				recv.read_int(); // always 32272
				recv.skip(5); // unused
				recv.read_int(); // mesos the merchant still holds
				recv.read_byte(); // always 0
				read_item_list(recv); // the saved items, without their position
				recv.skip(3); // unused
				break;
			case 0x24: // getFredrick(byte) on the one operation that carries data
				recv.skip(8); // unused
				break;
			default: // every other operation is answered with a single zero byte
				recv.read_byte(); // always 0
				break;
			}
		}

		// PacketCreator.openRPSNPC (PacketCreator.java:4945), rpsMesoError
		// (PacketCreator.java:4952), rpsSelection (PacketCreator.java:4961) and
		// rpsMode (PacketCreator.java:4969) - the leading byte selects the variant
		void parse_rps_game(InPacket& recv)
		{
			int8_t mode = recv.read_byte();

			switch (mode)
			{
			case 0x08: // openRPSNPC
				recv.read_int(); // npc id of the rps admin
				break;
			case 0x06: // rpsMesoError: the amount is only sent when it is known
				if (recv.available())
					recv.read_int(); // mesos missing for the next game
				break;
			case 0x0B: // rpsSelection
				recv.read_byte(); // selection of the player
				recv.read_byte(); // answer of the npc
				break;
			default: // rpsMode: the mode byte is the whole payload
				break;
			}
		}

		// PacketCreator.messengerInvite (PacketCreator.java:4345),
		// addMessengerPlayer (PacketCreator.java:4376), removeMessengerPlayer
		// (PacketCreator.java:4387), updateMessengerPlayer (PacketCreator.java:4394),
		// joinMessenger (PacketCreator.java:4405), messengerChat
		// (PacketCreator.java:4412) and messengerNote (PacketCreator.java:4419) - the
		// leading byte selects the variant
		void parse_messenger(InPacket& recv)
		{
			int8_t mode = recv.read_byte();

			switch (mode)
			{
			case 0x00: // addMessengerPlayer
			case 0x07: // updateMessengerPlayer
				recv.read_byte(); // position in the messenger
				read_char_look(recv); // appearance of the player
				recv.read_string(); // name of the player
				recv.read_byte(); // channel of the player
				recv.read_byte(); // always 0
				break;
			case 0x01: // joinMessenger
			case 0x02: // removeMessengerPlayer
				recv.read_byte(); // position in the messenger
				break;
			case 0x03: // messengerInvite
				recv.read_string(); // name of the inviter
				recv.read_byte(); // always 0
				recv.read_int(); // id of the messenger
				recv.read_byte(); // always 0
				break;
			case 0x06: // messengerChat
				recv.read_string(); // chat text
				break;
			default: // messengerNote, sent with mode 4 or 5 by this server
				recv.read_string(); // note text
				recv.read_byte(); // second mode of the note
				break;
			}
		}

		void register_packets7()
		{
			add(ServerOpcode::REMOVE_DOOR, parse_remove_door);
			add(ServerOpcode::SNOWBALL_STATE, parse_snowball_state);
			add(ServerOpcode::HIT_SNOWBALL, parse_hit_snowball);
			add(ServerOpcode::SNOWBALL_MESSAGE, parse_snowball_message);
			add(ServerOpcode::LEFT_KNOCK_BACK, parse_left_knock_back);
			add(ServerOpcode::COCONUT_HIT, parse_coconut_hit);
			add(ServerOpcode::COCONUT_SCORE, parse_coconut_score);
			add(ServerOpcode::GUILD_BOSS_HEALER_MOVE, parse_guild_boss_healer_move);
			add(ServerOpcode::GUILD_BOSS_PULLEY_STATE_CHANGE, parse_guild_boss_pulley_state_change);
			add(ServerOpcode::MONSTER_CARNIVAL_START, parse_monster_carnival_start);
			add(ServerOpcode::MONSTER_CARNIVAL_OBTAINED_CP, parse_monster_carnival_obtained_cp);
			add(ServerOpcode::MONSTER_CARNIVAL_PARTY_CP, parse_monster_carnival_party_cp);
			add(ServerOpcode::MONSTER_CARNIVAL_SUMMON, parse_monster_carnival_summon);
			add(ServerOpcode::MONSTER_CARNIVAL_MESSAGE, parse_monster_carnival_message);
			add(ServerOpcode::MONSTER_CARNIVAL_DIED, parse_monster_carnival_died);
			add(ServerOpcode::ARIANT_ARENA_USER_SCORE, parse_ariant_arena_user_score);
			add(ServerOpcode::SHEEP_RANCH_INFO, parse_sheep_ranch_info);
			add(ServerOpcode::SHEEP_RANCH_CLOTHES, parse_sheep_ranch_clothes);
			add(ServerOpcode::WITCH_TOWER_SCORE_UPDATE, parse_witch_tower_score_update);
			add(ServerOpcode::STORAGE, parse_storage);
			add(ServerOpcode::FREDRICK_MESSAGE, parse_fredrick_message);
			add(ServerOpcode::FREDRICK, parse_fredrick);
			add(ServerOpcode::RPS_GAME, parse_rps_game);
			add(ServerOpcode::MESSENGER, parse_messenger);
		}
	}
}
