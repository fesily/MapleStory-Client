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
		// 140 FIELD_OBSTACLE_ONOFF_LIST, 141 FIELD_OBSTACLE_ALL_RESET, 142 BLOW_WEATHER, 144 ADMIN_RESULT, 145 OX_QUIZ, 146 GMEVENT_INSTRUCTIONS, 147 CLOCK, 148 CONTI_MOVE, 149 CONTI_STATE, 154 STOP_CLOCK, 155 ARIANT_ARENA_SHOW_RESULT, 157 PYRAMID_GAUGE, 158 PYRAMID_SCORE, 159 QUICKSLOT_INIT, 164 CHALKBOARD, 165 UPDATE_CHAR_BOX, 170 MOVE_PET, 171 PET_CHAT, 172 PET_NAMECHANGE, 173 PET_EXCEPTION_LIST, 174 PET_COMMAND, 175 SPAWN_SPECIAL_MAPOBJECT, 176 REMOVE_SPECIAL_MAPOBJECT, 177 MOVE_SUMMON
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		// The server method commented on every parser is the emit site the layout was
		// taken from (all of them live in util/PacketCreator.java).
		namespace
		{
			// FIELD_OBSTACLE_ONOFF_LIST - PacketCreator.environmentMoveList
			// int count, then count times { string name, int mode }
			void parse_field_obstacle_onoff_list(InPacket& recv)
			{
				int32_t count = recv.read_int();

				for (int32_t i = 0; i < count; i++)
				{
					recv.read_string(); // environment name
					recv.read_int();    // 0 = stop and back to start, 1 = move
				}
			}

			// FIELD_OBSTACLE_ALL_RESET - PacketCreator.environmentMoveReset sends the
			// opcode without a payload
			void parse_field_obstacle_all_reset(InPacket&)
			{
			}

			// BLOW_WEATHER - PacketCreator.startMapEffect, removeMapEffect, hpqMessage
			// byte flag (startMapEffect sends it inverted), int itemid, then the message
			// when the effect is shown. The message has two encodings: a length prefixed
			// string (startMapEffect) and a fixed 13 byte string (hpqMessage), which are
			// told apart by the size of what is left of the payload
			void parse_blow_weather(InPacket& recv)
			{
				bool inactive = recv.read_bool();

				recv.read_int(); // itemid

				if (inactive || recv.length() == 0)
					return;

				if (recv.length() == 13)
					recv.skip_padded_string(13);
				else
					recv.read_string();
			}

			// ADMIN_RESULT - PacketCreator.getGMEffect, findMerchantResponse,
			// disableMinimap. The first byte picks one of three layouts
			void parse_admin_result(InPacket& recv)
			{
				int8_t type = recv.read_byte();

				switch (type)
				{
				case 0x13:
					// findMerchantResponse: byte which of the two values follows
					// (0 = mapid, 1 = channel), then a trailing zero byte
					if (recv.read_byte() == 0)
						recv.read_int();
					else
						recv.read_byte();

					recv.read_byte();
					break;
				case 0x1C:
					// disableMinimap sends the type as a short
					recv.read_byte();
					break;
				default:
					// getGMEffect: byte type, byte mode
					recv.read_byte();
					break;
				}
			}

			// OX_QUIZ - PacketCreator.showOXQuiz
			// byte ask question, byte question set, short question id
			void parse_ox_quiz(InPacket& recv)
			{
				recv.read_byte();  // asking a question (1) or showing the answer (0)
				recv.read_byte();  // question set
				recv.read_short(); // question id
			}

			// GMEVENT_INSTRUCTIONS - PacketCreator.showEventInstructions
			void parse_gmevent_instructions(InPacket& recv)
			{
				recv.read_byte();
			}

			// CLOCK - PacketCreator.getClock (type 2, int seconds) and getClockTime
			// (type 1, byte hour, byte minute, byte second)
			void parse_clock(InPacket& recv)
			{
				int8_t type = recv.read_byte();

				if (type == 1)
				{
					recv.read_byte(); // hour
					recv.read_byte(); // minute
					recv.read_byte(); // second
				}
				else
				{
					recv.read_int(); // seconds left
				}
			}

			// CONTI_MOVE - PacketCreator.crogBoatPacket
			// byte 10, byte 4 or 5 depending on the boat type
			void parse_conti_move(InPacket& recv)
			{
				recv.read_byte();
				recv.read_byte();
			}

			// CONTI_STATE - PacketCreator.boatPacket
			// byte 1 or 2 depending on the boat type, byte 0
			void parse_conti_state(InPacket& recv)
			{
				recv.read_byte();
				recv.read_byte();
			}

			// STOP_CLOCK - PacketCreator.removeClock
			void parse_stop_clock(InPacket& recv)
			{
				recv.read_byte();
			}

			// ARIANT_ARENA_SHOW_RESULT - PacketCreator.showAriantScoreBoard sends the
			// opcode without a payload
			void parse_ariant_arena_show_result(InPacket&)
			{
			}

			// PYRAMID_GAUGE - PacketCreator.pyramidGauge
			void parse_pyramid_gauge(InPacket& recv)
			{
				recv.read_int(); // gauge
			}

			// PYRAMID_SCORE - PacketCreator.pyramidScore and MassacreResult
			// byte rank, int exp gained
			void parse_pyramid_score(InPacket& recv)
			{
				recv.read_byte(); // rank, 0 = S up to 4 = D
				recv.read_int();  // exp gained
			}

			// QUICKSLOT_INIT - PacketCreator.QuickslotMappedInit, which encodes a
			// QuickslotBinding: a bool, then QuickslotBinding.QUICKSLOT_SIZE ints when
			// the quickslots are not the defaults
			void parse_quickslot_init(InPacket& recv)
			{
				if (!recv.read_bool())
					return;

				for (int i = 0; i < 8; i++)
					recv.read_int();
			}

			// CHALKBOARD - PacketCreator.useChalkboard
			// int character id, byte 0 when the board is closed, otherwise byte 1 and
			// the text of the board
			void parse_chalkboard(InPacket& recv)
			{
				recv.read_int(); // character id

				if (recv.read_byte() == 0)
					return;

				recv.read_string();
			}

			// UPDATE_CHAR_BOX - PacketCreator.updatePlayerShopBox, removePlayerShopBox,
			// addOmokBox, addMatchCardBox, removeMinigameBox. The box info written by
			// updatePlayerShopBoxInfo/addAnnounceBox is int object id, string
			// description and five bytes (password, item or piece, players, capacity,
			// joinable)
			void parse_update_char_box(InPacket& recv)
			{
				recv.read_int(); // character id

				int8_t type = recv.read_byte();

				// A zero type closes the box and carries nothing else. addAnnounceBox
				// writes the game type here instead, which is zero for the (unused)
				// UNDEFINED mini game type, so a block can still follow a zero
				if (type == 0 && recv.length() == 0)
					return;

				recv.read_int();    // object id
				recv.read_string(); // description
				recv.skip(5);       // password, item or piece, players, capacity, joinable
			}

			// The movement list of MOVE_PET is written by
			// PacketCreator.serializeMovementList: a byte command count, then every
			// fragment as re-serialized by the classes of org.gms.server.movement. The
			// command byte selects the layout of its fragment
			void skip_life_movement_list(InPacket& recv)
			{
				uint8_t count = static_cast<uint8_t>(recv.read_byte());

				for (uint8_t i = 0; i < count; i++)
				{
					int8_t command = recv.read_byte();

					switch (command)
					{
					case 0: // AbsoluteLifeMovement
					case 5:
					case 17:
						recv.skip(13); // position, wobble, foothold, stance, duration
						break;
					case 1: // RelativeLifeMovement
					case 2:
					case 6:
					case 12:
					case 13:
					case 16:
					case 18:
					case 19:
					case 20:
					case 22:
						recv.skip(7); // position, stance, duration
						break;
					case 3: // TeleportMovement
					case 4:
					case 7:
					case 8:
					case 9:
					case 11:
						recv.skip(9); // position, wobble, stance
						break;
					case 10: // ChangeEquip
						recv.skip(1); // window slot
						break;
					case 15: // JumpDownMovement
						recv.skip(15); // position, wobble, foothold, origin foothold, stance, duration
						break;
					default:
						// Commands 14 and 21 are skipped as raw bytes by the server
						// parser and never reach the serializer, everything else is
						// refused there, so this fragment cannot be part of the packet
						throw PacketError("Unknown movement command " + std::to_string(command));
					}
				}
			}

			// MOVE_PET - PacketCreator.movePet
			// int character id, byte pet slot, int pet id, then the movement list
			void parse_move_pet(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_byte(); // pet slot
				recv.read_int();  // pet id

				skip_life_movement_list(recv);
			}

			// PET_CHAT - PacketCreator.petChat
			// int character id, byte pet slot, byte 0, byte act, string text, bool chat
			// balloon
			void parse_pet_chat(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_byte(); // pet slot
				recv.read_byte(); // 0
				recv.read_byte(); // act
				recv.read_string();
				recv.read_bool(); // chat balloon
			}

			// PET_NAMECHANGE - PacketCreator.changePetName
			// int character id, byte pet slot, string new name, bool name tag
			void parse_pet_namechange(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_byte(); // pet slot
				recv.read_string();
				recv.read_bool(); // name tag
			}

			// PET_EXCEPTION_LIST - PacketCreator.loadExceptionList
			// int character id, byte pet slot, long pet id, byte count, then the item
			// ids the pet ignores
			void parse_pet_exception_list(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_byte(); // pet slot
				recv.read_long(); // pet id

				uint8_t count = static_cast<uint8_t>(recv.read_byte());

				for (uint8_t i = 0; i < count; i++)
					recv.read_int();
			}

			// PET_COMMAND - PacketCreator.petFoodResponse (byte 1) and commandResponse
			// (byte 0). Both start with int character id and byte pet slot
			void parse_pet_command(InPacket& recv)
			{
				recv.read_int();  // character id
				recv.read_byte(); // pet slot

				if (recv.read_byte() == 1)
				{
					recv.read_bool(); // success
					recv.read_bool(); // chat balloon
				}
				else
				{
					recv.read_byte(); // animation
					recv.read_bool(); // !talk
					recv.read_bool(); // chat balloon
				}
			}

			// SPAWN_SPECIAL_MAPOBJECT - PacketCreator.spawnSummon
			// int owner id, int object id, int skill id, byte 0x0A, byte skill level,
			// position, byte stance, short 0, byte movement type, bool attackable,
			// bool !animated
			void parse_spawn_special_mapobject(InPacket& recv)
			{
				recv.read_int();   // owner id
				recv.read_int();   // object id
				recv.read_int();   // skill id
				recv.read_byte();  // 0x0A, unused
				recv.read_byte();  // skill level
				recv.read_point(); // position
				recv.read_byte();  // stance, which includes the foothold
				recv.read_short(); // 0
				recv.read_byte();  // movement type
				recv.read_bool();  // attackable
				recv.read_bool();  // !animated
			}

			// REMOVE_SPECIAL_MAPOBJECT - PacketCreator.removeSummon
			// int owner id, int object id, byte 4 for an animated removal and 1 otherwise
			void parse_remove_special_mapobject(InPacket& recv)
			{
				recv.read_int();  // owner id
				recv.read_int();  // object id
				recv.read_byte(); // animation
			}

			// MOVE_SUMMON - PacketCreator.moveSummon
			// int character id, int object id, position, then the movement bytes of the
			// client packet, which the server copies unchanged (rebroadcastMovementList)
			void parse_move_summon(InPacket& recv)
			{
				recv.read_int();   // character id
				recv.read_int();   // object id
				recv.read_point(); // start position
				recv.skip(recv.length()); // movement list, echoed byte for byte
			}
		}

		void register_packets4()
		{
			add(ServerOpcode::FIELD_OBSTACLE_ONOFF_LIST, parse_field_obstacle_onoff_list);
			add(ServerOpcode::FIELD_OBSTACLE_ALL_RESET, parse_field_obstacle_all_reset);
			add(ServerOpcode::BLOW_WEATHER, parse_blow_weather);
			add(ServerOpcode::ADMIN_RESULT, parse_admin_result);
			add(ServerOpcode::OX_QUIZ, parse_ox_quiz);
			add(ServerOpcode::GMEVENT_INSTRUCTIONS, parse_gmevent_instructions);
			add(ServerOpcode::CLOCK, parse_clock);
			add(ServerOpcode::CONTI_MOVE, parse_conti_move);
			add(ServerOpcode::CONTI_STATE, parse_conti_state);
			add(ServerOpcode::STOP_CLOCK, parse_stop_clock);
			add(ServerOpcode::ARIANT_ARENA_SHOW_RESULT, parse_ariant_arena_show_result);
			add(ServerOpcode::PYRAMID_GAUGE, parse_pyramid_gauge);
			add(ServerOpcode::PYRAMID_SCORE, parse_pyramid_score);
			add(ServerOpcode::QUICKSLOT_INIT, parse_quickslot_init);
			add(ServerOpcode::CHALKBOARD, parse_chalkboard);
			add(ServerOpcode::UPDATE_CHAR_BOX, parse_update_char_box);
			add(ServerOpcode::MOVE_PET, parse_move_pet);
			add(ServerOpcode::PET_CHAT, parse_pet_chat);
			add(ServerOpcode::PET_NAMECHANGE, parse_pet_namechange);
			add(ServerOpcode::PET_EXCEPTION_LIST, parse_pet_exception_list);
			add(ServerOpcode::PET_COMMAND, parse_pet_command);
			add(ServerOpcode::SPAWN_SPECIAL_MAPOBJECT, parse_spawn_special_mapobject);
			add(ServerOpcode::REMOVE_SPECIAL_MAPOBJECT, parse_remove_special_mapobject);
			add(ServerOpcode::MOVE_SUMMON, parse_move_summon);
		}
	}
}
