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
		// 178 SUMMON_ATTACK, 179 DAMAGE_SUMMON, 180 SUMMON_SKILL, 181 SPAWN_DRAGON, 182 MOVE_DRAGON, 183 REMOVE_DRAGON, 190 SKILL_EFFECT, 191 CANCEL_SKILL_EFFECT, 192 DAMAGE_PLAYER, 193 FACIAL_EXPRESSION, 194 SHOW_ITEM_EFFECT, 196 SHOW_CHAIR, 199 GIVE_FOREIGN_BUFF, 200 CANCEL_FOREIGN_BUFF, 201 UPDATE_PARTYMEMBER_HP, 202 GUILD_NAME_CHANGED, 203 GUILD_MARK_CHANGED, 204 THROW_GRENADE, 205 CANCEL_CHAIR, 207 DOJO_WARP_UP, 211 UPDATE_QUEST_INFO, 212 ON_NOTIFY_HP_DEC_BY_FIELD, 214 PLAYER_HINT, 217 MAKER_RESULT
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		// Two emitters append a block whose shape is decided by server-side state that
		// never reaches the wire (GIVE_FOREIGN_BUFF buff values, DAMAGE_PLAYER's pgmr
		// and fake blocks); those parsers verify the fixed part and then consume the
		// block as a whole, which is noted at the parser.
		// Unsupported::forward() logs the parsed byte count and states that the
		// capability itself is not implemented.

		namespace
		{
			// Drops the rest of a payload whose size the server does not put on the wire
			void skip_remaining(InPacket& recv)
			{
				recv.skip(recv.length());
			}

			// PacketCreator.summonAttack (PacketCreator.java:2294)
			// cid, oid of the summon, character level, direction, number of hits, then
			// that many (monster oid, constant byte 6, damage) entries
			void parse_summon_attack(InPacket& recv)
			{
				recv.read_int();   // cid
				recv.read_int();   // oid of the summon
				recv.read_byte();  // character level, always written as 0
				recv.read_byte();  // direction

				uint8_t size = recv.read_byte();

				for (uint8_t i = 0; i < size; i++)
				{
					recv.read_int();   // oid of the monster hit
					recv.read_byte();  // constant 6
					recv.read_int();   // damage dealt
				}
			}

			// PacketCreator.damageSummon (PacketCreator.java:4067)
			void parse_damage_summon(InPacket& recv)
			{
				recv.read_int();   // cid
				recv.read_int();   // oid of the summon
				recv.read_byte();  // constant 12
				recv.read_int();   // damage taken
				recv.read_int();   // oid of the monster the damage came from
				recv.read_byte();  // constant 0
			}

			// PacketCreator.summonSkill (PacketCreator.java:4571)
			void parse_summon_skill(InPacket& recv)
			{
				recv.read_int();   // cid
				recv.read_int();   // skill id of the summon
				recv.read_byte();  // new stance
			}

			// PacketCreator.spawnDragon (PacketCreator.java:7376)
			void parse_spawn_dragon(InPacket& recv)
			{
				recv.read_int();    // owner character id, doubles as the oid of the dragon
				recv.read_short();  // x of the dragon position
				recv.read_short();  // constant 0
				recv.read_short();  // y of the dragon position
				recv.read_short();  // constant 0
				recv.read_byte();   // stance
				recv.read_byte();   // constant 0
				recv.read_short();  // job id of the owner
			}

			// PacketCreator.moveDragon (PacketCreator.java:7389)
			// owner id and start position, then the movement list the server rebroadcasts
			// byte for byte from the client, without writing a length for it
			void parse_move_dragon(InPacket& recv)
			{
				recv.read_int();    // owner character id
				recv.read_point();  // start position

				skip_remaining(recv);  // movement list
			}

			// PacketCreator.removeDragon (PacketCreator.java:7403)
			void parse_remove_dragon(InPacket& recv)
			{
				recv.read_int();  // character id of the owner
			}

			// PacketCreator.skillEffect (PacketCreator.java:4285)
			void parse_skill_effect(InPacket& recv)
			{
				recv.read_int();   // cid
				recv.read_int();   // skill id
				recv.read_byte();  // skill level
				recv.read_byte();  // flags
				recv.read_byte();  // speed
				recv.read_byte();  // direction
			}

			// PacketCreator.skillCancel (PacketCreator.java:4296)
			void parse_cancel_skill_effect(InPacket& recv)
			{
				recv.read_int();  // cid
				recv.read_int();  // skill id
			}

			// PacketCreator.damagePlayer (PacketCreator.java:2598)
			// cid, skill, damage and then a tail which depends on the skill and on the
			// pgmr and fake arguments the server never sends:
			//   skill != -4: monster oid, direction, either 12 bytes (pgmr) or a
			//                constant 0 short, the repeated damage and an optional fake
			//                damage int
			//   skill == -4: only the repeated damage
			// The field in front of the tail is parsed, the tail itself is consumed as
			// a whole because its length is not on the wire.
			void parse_damage_player(InPacket& recv)
			{
				recv.read_int();  // cid

				int8_t skill = recv.read_byte();

				if (skill == -3)
					recv.read_int();  // constant 0

				recv.read_int();  // damage

				if (skill == -4)
				{
					recv.read_int();  // damage, written twice for this skill
					return;
				}

				recv.read_int();   // oid of the monster the damage came from
				recv.read_byte();  // direction

				skip_remaining(recv);  // pgmr block, repeated damage, optional fake damage
			}

			// PacketCreator.facialExpression (PacketCreator.java:2240)
			void parse_facial_expression(InPacket& recv)
			{
				recv.read_int();  // cid
				recv.read_int();  // expression id
			}

			// PacketCreator.itemEffect (PacketCreator.java:4148)
			void parse_show_item_effect(InPacket& recv)
			{
				recv.read_int();  // cid
				recv.read_int();  // item id of the effect
			}

			// PacketCreator.showChair (PacketCreator.java:4162)
			void parse_show_chair(InPacket& recv)
			{
				recv.read_int();  // cid
				recv.read_int();  // item id of the chair
			}

			// GIVE_FOREIGN_BUFF has seven emitters in this build, all of which start
			// with the cid and the two buff masks:
			//   PacketCreator.showMonsterRiding (2830), giveForeignDebuff (2963),
			//   giveForeignBuff (2995), giveForeignSlowDebuff (3078),
			//   giveForeignChairSkillEffect (3106), giveForeignWKChargeEffect (3125)
			//   and giveForeignPirateBuff (5431)
			// What follows is a buff value block whose shape and size follow from the
			// buff list the server holds, so the emitter cannot be recovered from the
			// packet; the block is consumed as a whole.
			void parse_give_foreign_buff(InPacket& recv)
			{
				recv.read_int();   // cid
				recv.read_long();  // first buff mask
				recv.read_long();  // second buff mask

				skip_remaining(recv);  // buff values, shape depends on the emitter
			}

			// CANCEL_FOREIGN_BUFF has five emitters and every one of them writes the
			// cid followed by the two buff masks and nothing else:
			//   PacketCreator.cancelForeignFirstDebuff (2979), cancelForeignDebuff (2987),
			//   cancelForeignBuff (3007), cancelForeignSlowDebuff (3093) and
			//   cancelForeignChairSkillEffect (3136)
			void parse_cancel_foreign_buff(InPacket& recv)
			{
				recv.read_int();   // cid
				recv.read_long();  // first buff mask
				recv.read_long();  // second buff mask
			}

			// PacketCreator.updatePartyMemberHP (PacketCreator.java:3942)
			void parse_update_party_member_hp(InPacket& recv)
			{
				recv.read_int();  // cid
				recv.read_int();  // current hp
				recv.read_int();  // max hp
			}

			// GuildPackets.guildNameChanged (GuildPackets.java:531)
			void parse_guild_name_changed(InPacket& recv)
			{
				recv.read_int();     // character id
				recv.read_string();  // guild name, uint16 byte length + bytes
			}

			// GuildPackets.guildMarkChanged (GuildPackets.java:538)
			void parse_guild_mark_changed(InPacket& recv)
			{
				recv.read_int();    // character id
				recv.read_short();  // background
				recv.read_byte();   // background colour
				recv.read_short();  // mark
				recv.read_byte();   // mark colour
			}

			// PacketCreator.throwGrenade (PacketCreator.java:2381)
			// all four trailing fields are ints, the position is not a point
			void parse_throw_grenade(InPacket& recv)
			{
				recv.read_int();  // cid
				recv.read_int();  // x of the position
				recv.read_int();  // y of the position
				recv.read_int();  // key down
				recv.read_int();  // skill id
				recv.read_int();  // skill level
			}

			// PacketCreator.cancelChair (PacketCreator.java:4169)
			// the leading byte selects the layout: 0 when no chair is used, 1 followed
			// by the chair id otherwise
			void parse_cancel_chair(InPacket& recv)
			{
				int8_t has_chair = recv.read_byte();

				if (has_chair == 1)
					recv.read_short();  // chair id
			}

			// PacketCreator.dojoWarpUp (PacketCreator.java:6729)
			void parse_dojo_warp_up(InPacket& recv)
			{
				recv.read_byte();  // constant 0
				recv.read_byte();  // constant 6
			}

			// UPDATE_QUEST_INFO is shared by seven emitters which all start with a
			// sub-type byte that selects the layout:
			//   PacketCreator.updateQuestInfo (2886), addQuestTimeLimit (2901),
			//   removeQuestTimeLimit (2910), updateQuestFinish (6272), questError (6288),
			//   questFailure (6295) and questExpire (6301)
			void parse_update_quest_info(InPacket& recv)
			{
				int8_t type = recv.read_byte();

				switch (type)
				{
				case 6:  // addQuestTimeLimit
					recv.read_short();  // size of the quest list, always written as 1
					recv.read_short();  // quest id
					recv.read_int();    // time limit in seconds
					break;
				case 7:  // removeQuestTimeLimit
					recv.read_short();  // position, always written as 1
					recv.read_short();  // quest id
					break;
				case 8:  // updateQuestInfo and updateQuestFinish
					recv.read_short();  // quest id
					recv.read_int();    // npc id

					// Both emitters write the same sub-type, but updateQuestFinish
					// ends with the next quest id (a short) where updateQuestInfo
					// ends with a constant 0 (an int), so the remaining length of the
					// payload tells the two apart
					if (recv.length() == 2)
						recv.read_short();  // next quest id
					else
						recv.read_int();    // constant 0
					break;
				case 0x0A:  // questError
					recv.read_short();  // quest id
					break;
				case 0x0F:  // questExpire
					recv.read_short();  // quest id
					break;
				case 0x0B:  // questFailure: the failure reason is the sub-type itself,
				case 0x0D:  // which is all this emitter writes (no caller in this build)
				case 0x0E:
					break;
				default:
					skip_remaining(recv);  // sub-type without a known layout
					break;
				}
			}

			// PacketCreator.onNotifyHPDecByField (PacketCreator.java:2895)
			void parse_on_notify_hp_dec_by_field(InPacket& recv)
			{
				recv.read_int();  // hp change, negative when the field takes hp
			}

			// PacketCreator.sendHint (PacketCreator.java:4326)
			void parse_player_hint(InPacket& recv)
			{
				recv.read_string();  // hint text, uint16 byte length + bytes
				recv.read_short();   // width
				recv.read_short();   // height
				recv.read_byte();    // constant 1
			}

			// MAKER_RESULT is shared by four emitters; the first int is the result and
			// the second one is the mode which selects the body:
			//   PacketCreator.makerResult (6309), makerResultCrystal (6338),
			//   makerResultDesynth (6347) and makerEnableActions (6361)
			void parse_maker_result(InPacket& recv)
			{
				recv.read_int();  // result, 0 = success, 1 = failure

				int32_t mode = recv.read_int();

				switch (mode)
				{
				case 0:  // makerEnableActions
					recv.read_int();  // constant 0
					recv.read_int();  // constant 0
					break;
				case 1:  // makerResult
				{
					bool failed = recv.read_bool();

					if (!failed)
					{
						recv.read_int();  // item that was made
						recv.read_int();  // amount that was made
					}

					int32_t lost = recv.read_int();

					for (int32_t i = 0; i < lost; i++)
					{
						recv.read_int();  // item id that was used up
						recv.read_int();  // amount that was used up
					}

					int32_t gems = recv.read_int();

					for (int32_t i = 0; i < gems; i++)
						recv.read_int();  // buff gem that was used up

					if (recv.read_bool())
						recv.read_int();  // item id of the stimulator

					recv.read_int();  // mesos spent
					break;
				}
				case 3:  // makerResultCrystal
					recv.read_int();  // item gained
					recv.read_int();  // item lost
					break;
				case 4:  // makerResultDesynth
				{
					recv.read_int();  // item that was desynthesised

					int32_t gained = recv.read_int();

					for (int32_t i = 0; i < gained; i++)
					{
						recv.read_int();  // item id that was gained
						recv.read_int();  // amount that was gained
					}

					recv.read_int();  // mesos spent
					break;
				}
				default:
					skip_remaining(recv);  // mode without a known layout
					break;
				}
			}
		}

		void register_packets5()
		{
			add(ServerOpcode::SUMMON_ATTACK, parse_summon_attack);
			add(ServerOpcode::DAMAGE_SUMMON, parse_damage_summon);
			add(ServerOpcode::SUMMON_SKILL, parse_summon_skill);
			add(ServerOpcode::SPAWN_DRAGON, parse_spawn_dragon);
			add(ServerOpcode::MOVE_DRAGON, parse_move_dragon);
			add(ServerOpcode::REMOVE_DRAGON, parse_remove_dragon);
			add(ServerOpcode::SKILL_EFFECT, parse_skill_effect);
			add(ServerOpcode::CANCEL_SKILL_EFFECT, parse_cancel_skill_effect);
			add(ServerOpcode::DAMAGE_PLAYER, parse_damage_player);
			add(ServerOpcode::FACIAL_EXPRESSION, parse_facial_expression);
			add(ServerOpcode::SHOW_ITEM_EFFECT, parse_show_item_effect);
			add(ServerOpcode::SHOW_CHAIR, parse_show_chair);
			add(ServerOpcode::GIVE_FOREIGN_BUFF, parse_give_foreign_buff);
			add(ServerOpcode::CANCEL_FOREIGN_BUFF, parse_cancel_foreign_buff);
			add(ServerOpcode::UPDATE_PARTYMEMBER_HP, parse_update_party_member_hp);
			add(ServerOpcode::GUILD_NAME_CHANGED, parse_guild_name_changed);
			add(ServerOpcode::GUILD_MARK_CHANGED, parse_guild_mark_changed);
			add(ServerOpcode::THROW_GRENADE, parse_throw_grenade);
			add(ServerOpcode::CANCEL_CHAIR, parse_cancel_chair);
			add(ServerOpcode::DOJO_WARP_UP, parse_dojo_warp_up);
			add(ServerOpcode::UPDATE_QUEST_INFO, parse_update_quest_info);
			add(ServerOpcode::ON_NOTIFY_HP_DEC_BY_FIELD, parse_on_notify_hp_dec_by_field);
			add(ServerOpcode::PLAYER_HINT, parse_player_hint);
			add(ServerOpcode::MAKER_RESULT, parse_maker_result);
		}
	}
}