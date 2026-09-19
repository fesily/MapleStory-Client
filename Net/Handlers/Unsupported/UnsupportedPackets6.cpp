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
		// 220 OPEN_UI, 221 LOCK_UI, 222 DISABLE_UI, 223 SPAWN_GUIDE, 224 TALK_GUIDE, 225 SHOW_COMBO, 240 MOVE_MONSTER_RESPONSE, 242 APPLY_MONSTER_STATUS, 243 CANCEL_MONSTER_STATUS, 246 DAMAGE_MONSTER, 251 CATCH_MONSTER, 252 CATCH_MONSTER_WITH_ITEM, 258 REMOVE_NPC, 260 NPC_ACTION, 263 SET_NPC_SCRIPTABLE, 265 SPAWN_HIRED_MERCHANT, 266 DESTROY_HIRED_MERCHANT, 267 UPDATE_HIRED_MERCHANT, 270 CANNOT_SPAWN_KITE, 271 SPAWN_KITE, 272 REMOVE_KITE, 273 SPAWN_MIST, 274 REMOVE_MIST, 275 SPAWN_DOOR
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		// The "capability not implemented" log line is written once per packet by
		// Unsupported::forward, together with the byte counts of the parser.

		namespace
		{
			// Bytes of the NPC talk reply, the only NPC_ACTION layout with a fixed size
			const size_t NPC_TALK_PAYLOAD = 6;
			// Smallest tally of the tail of APPLY_MONSTER_STATUS: count byte and trailing int
			const size_t APPLY_STATUS_TAIL = 5;

			// Number of set bits of a monster status mask, one status per bit
			int32_t count_bits(int32_t mask)
			{
				int32_t count = 0;

				while (mask != 0)
				{
					count += mask & 1;
					mask = static_cast<int32_t>(static_cast<uint32_t>(mask) >> 1);
				}

				return count;
			}

			// Drop the rest of a payload whose layout the server does not fix on the wire
			void skip_remaining(InPacket& recv)
			{
				recv.skip(recv.length());
			}
		}

		// PacketCreator.openUI (PacketCreator.java:6113) - id of the UI to open
		void parse_open_ui(InPacket& recv)
		{
			recv.read_byte(); // ui
		}

		// PacketCreator.lockUI (PacketCreator.java:6119)
		void parse_lock_ui(InPacket& recv)
		{
			recv.read_bool(); // enable
		}

		// PacketCreator.disableUI (PacketCreator.java:6125)
		void parse_disable_ui(InPacket& recv)
		{
			recv.read_bool(); // enable
		}

		// PacketCreator.spawnGuide (PacketCreator.java:6925)
		void parse_spawn_guide(InPacket& recv)
		{
			recv.read_bool(); // spawn
		}

		// PacketCreator.talkGuide and PacketCreator.guideHint (PacketCreator.java:6931, 6939)
		// Sub type 0 carries the talk text and eight fixed bytes, sub type 1 carries the
		// hint id and its duration
		void parse_talk_guide(InPacket& recv)
		{
			int8_t type = recv.read_byte(); // sub type

			switch (type)
			{
			case 0:
				recv.read_string(); // talk
				recv.skip(8); // 0xC8 00 00 00 0xA0 0x0F 00 00
				break;
			case 1:
				recv.read_int(); // hint
				recv.read_int(); // duration, always 7000
				break;
			default:
				// This server emits no other sub type
				skip_remaining(recv);
				break;
			}
		}

		// PacketCreator.showCombo (PacketCreator.java:7280)
		void parse_show_combo(InPacket& recv)
		{
			recv.read_int(); // combo count
		}

		// PacketCreator.moveMonsterResponse (PacketCreator.java:1630)
		void parse_move_monster_response(InPacket& recv)
		{
			recv.read_int(); // object id of the monster
			recv.read_short(); // move id
			recv.read_bool(); // use skills
			recv.read_short(); // current mp
			recv.read_byte(); // skill id
			recv.read_byte(); // skill level
		}

		// PacketCreator.applyMonsterStatus (PacketCreator.java:3980)
		// The two masks hold one bit per status, so they fix the number of stat entries
		// which follow: a short value, a four byte skill id (or a mob skill id as two
		// shorts) and a final -1 each
		void parse_apply_monster_status(InPacket& recv)
		{
			recv.read_int(); // object id
			recv.read_long(); // always 0

			int32_t firstmask = recv.read_int();
			int32_t secondmask = recv.read_int();
			int32_t entries = count_bits(firstmask) + count_bits(secondmask);

			for (int32_t i = 0; i < entries; i++)
			{
				recv.read_short(); // status value
				recv.read_int(); // skill id, or mob skill type and level as two shorts
				recv.read_short(); // buff time, always -1
			}

			// The reflection values are not counted on the wire, but the tail of the
			// packet is: four bytes per reflection, then the status count byte and a
			// trailing int
			size_t tail = recv.length();

			if (tail >= APPLY_STATUS_TAIL && (tail - APPLY_STATUS_TAIL) % 4 == 0)
			{
				recv.skip(((tail - APPLY_STATUS_TAIL) / 4) * 4); // reflection values
				recv.read_byte(); // status count, halved by the server when reflections follow
				recv.read_int(); // always 0
			}
			else
			{
				skip_remaining(recv);
			}
		}

		// PacketCreator.cancelMonsterStatus (PacketCreator.java:4009)
		void parse_cancel_monster_status(InPacket& recv)
		{
			recv.read_int(); // object id
			recv.read_long(); // always 0
			recv.read_int(); // first mask
			recv.read_int(); // second mask
			recv.read_int(); // always 0
		}

		// PacketCreator.damageMonster and PacketCreator.healMonster (PacketCreator.java:4086),
		// also used by PacketCreator.MobDamageMobFriendly (PacketCreator.java:6751); the byte
		// is 0 for player damage and heals, 1 when a monster damages another monster
		void parse_damage_monster(InPacket& recv)
		{
			recv.read_int(); // object id
			recv.read_byte(); // direction, 0 or 1
			recv.read_int(); // damage, negative for heals
			recv.read_int(); // remaining hp
			recv.read_int(); // max hp
		}

		// PacketCreator.catchMonster (PacketCreator.java:4303)
		void parse_catch_monster(InPacket& recv)
		{
			recv.read_int(); // object id of the monster
			recv.read_byte(); // success
		}

		// PacketCreator.catchMonster with an item id (PacketCreator.java:4310)
		void parse_catch_monster_with_item(InPacket& recv)
		{
			recv.read_int(); // object id of the monster
			recv.read_int(); // item id of the reward
			recv.read_byte(); // success
		}

		// PacketCreator.removeNPC (PacketCreator.java:6146)
		void parse_remove_npc(InPacket& recv)
		{
			recv.read_int(); // object id of the npc
		}

		// NPCAnimationHandler.handlePacket (NPCAnimationHandler.java:37)
		// The server copies the request into the reply: an npc talk is fixed at six
		// bytes, an npc move is the request bytes without their nine byte header
		void parse_npc_action(InPacket& recv)
		{
			if (recv.length() == NPC_TALK_PAYLOAD)
			{
				recv.read_int(); // object id of the npc
				recv.read_byte(); // talk index
				recv.read_byte(); // talk index
				return;
			}

			skip_remaining(recv);
		}

		// PacketCreator.setNPCScriptable (PacketCreator.java:7435)
		// The name is written as a byte length prefixed string on both charset branches
		void parse_set_npc_scriptable(InPacket& recv)
		{
			int8_t count = recv.read_byte();

			for (int8_t i = 0; i < count; i++)
			{
				recv.read_int(); // npc id
				recv.read_string(); // npc name shown by the client under etc
				recv.read_int(); // start time, always 0
				recv.read_int(); // end time, always Integer.MAX_VALUE
			}
		}

		// PacketCreator.spawnHiredMerchantBox (PacketCreator.java:5315)
		void parse_spawn_hired_merchant(InPacket& recv)
		{
			recv.read_int(); // owner id
			recv.read_int(); // item id of the box
			recv.read_short(); // x
			recv.read_short(); // y
			recv.read_short(); // always 0
			recv.read_string(); // owner name
			recv.read_byte(); // always 0x05
			recv.read_int(); // object id
			recv.read_string(); // description
			recv.read_byte(); // item id modulo 100, the box appearance
			recv.read_byte(); // always 1
			recv.read_byte(); // always 4
		}

		// PacketCreator.removeHiredMerchantBox (PacketCreator.java:5331)
		void parse_destroy_hired_merchant(InPacket& recv)
		{
			recv.read_int(); // object id of the box
		}

		// PacketCreator.updateHiredMerchantBox (PacketCreator.java:2206), whose
		// updateHiredMerchantBoxInfo writes the marker byte 5 and the two visitor
		// bytes of HiredMerchant.getShopRoomInfo (HiredMerchant.java:124)
		void parse_update_hired_merchant(InPacket& recv)
		{
			recv.read_int(); // owner id
			recv.read_byte(); // always 5
			recv.read_int(); // object id of the box
			recv.read_string(); // description
			recv.read_byte(); // item id modulo 100
			recv.read_byte(); // visitor count
			recv.read_byte(); // visitor capacity
		}

		// PacketCreator.sendCannotSpawnKite (PacketCreator.java:1187) - empty payload
		void parse_cannot_spawn_kite(InPacket&)
		{
		}

		// PacketCreator.spawnKite (PacketCreator.java:1169)
		void parse_spawn_kite(InPacket& recv)
		{
			recv.read_int(); // object id
			recv.read_int(); // item id
			recv.read_string(); // message on the kite
			recv.read_string(); // owner name
			recv.read_short(); // x
			recv.read_short(); // foothold id
		}

		// PacketCreator.removeKite (PacketCreator.java:1180)
		void parse_remove_kite(InPacket& recv)
		{
			recv.read_byte(); // animation type, 0 finishes the animation, 1 removes it at once
			recv.read_int(); // object id
		}

		// PacketCreator.spawnMist (PacketCreator.java:4045)
		void parse_spawn_mist(InPacket& recv)
		{
			recv.read_int(); // object id of the mist
			recv.read_int(); // 0 mob, 1 poison, 2 smokescreen, 4 recovery
			recv.read_int(); // owner id
			recv.read_int(); // skill id
			recv.read_byte(); // skill level
			recv.read_short(); // skill delay
			recv.read_int(); // box left
			recv.read_int(); // box top
			recv.read_int(); // box right
			recv.read_int(); // box bottom
			recv.read_int(); // always 0
		}

		// PacketCreator.removeMist (PacketCreator.java:4061)
		void parse_remove_mist(InPacket& recv)
		{
			recv.read_int(); // object id of the mist
		}

		// PacketCreator.spawnDoor (PacketCreator.java:1102)
		void parse_spawn_door(InPacket& recv)
		{
			recv.read_bool(); // launched, false while the door is being placed
			recv.read_int(); // owner id
			recv.read_point(); // position
		}

		void register_packets6()
		{
			add(ServerOpcode::OPEN_UI, parse_open_ui);
			add(ServerOpcode::LOCK_UI, parse_lock_ui);
			add(ServerOpcode::DISABLE_UI, parse_disable_ui);
			add(ServerOpcode::SPAWN_GUIDE, parse_spawn_guide);
			add(ServerOpcode::TALK_GUIDE, parse_talk_guide);
			add(ServerOpcode::SHOW_COMBO, parse_show_combo);
			add(ServerOpcode::MOVE_MONSTER_RESPONSE, parse_move_monster_response);
			add(ServerOpcode::APPLY_MONSTER_STATUS, parse_apply_monster_status);
			add(ServerOpcode::CANCEL_MONSTER_STATUS, parse_cancel_monster_status);
			add(ServerOpcode::DAMAGE_MONSTER, parse_damage_monster);
			add(ServerOpcode::CATCH_MONSTER, parse_catch_monster);
			add(ServerOpcode::CATCH_MONSTER_WITH_ITEM, parse_catch_monster_with_item);
			add(ServerOpcode::REMOVE_NPC, parse_remove_npc);
			add(ServerOpcode::NPC_ACTION, parse_npc_action);
			add(ServerOpcode::SET_NPC_SCRIPTABLE, parse_set_npc_scriptable);
			add(ServerOpcode::SPAWN_HIRED_MERCHANT, parse_spawn_hired_merchant);
			add(ServerOpcode::DESTROY_HIRED_MERCHANT, parse_destroy_hired_merchant);
			add(ServerOpcode::UPDATE_HIRED_MERCHANT, parse_update_hired_merchant);
			add(ServerOpcode::CANNOT_SPAWN_KITE, parse_cannot_spawn_kite);
			add(ServerOpcode::SPAWN_KITE, parse_spawn_kite);
			add(ServerOpcode::REMOVE_KITE, parse_remove_kite);
			add(ServerOpcode::SPAWN_MIST, parse_spawn_mist);
			add(ServerOpcode::REMOVE_MIST, parse_remove_mist);
			add(ServerOpcode::SPAWN_DOOR, parse_spawn_door);
		}
	}
}