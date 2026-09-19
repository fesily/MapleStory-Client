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
#include "MapObjectHandlers.h"

#include "Helpers/LoginParser.h"
#include "Helpers/MovementParser.h"

#include "../../Gameplay/Stage.h"
#include "../../MapleStory.h"

#include <iostream>

namespace ms
{
	namespace
	{
		// SPAWN_PLAYER writes the buff value only when COMBO or MORPH is active
		// (PacketCreator.java:1863). COMBO is bit 0x200000 of the high mask word
		// (BuffStat.java: COMBO(0x20000000000000L)); MORPH is the separate int read
		// in front of the mask and is not part of the mask itself.
		constexpr int32_t FOREIGN_BUFF_COMBO = 0x200000;

		// Mirrors PacketCreator.encodeTemporary (PacketCreator.java:1432-1473).
		// Used by SPAWN_MONSTER_CONTROL(238) and by makeMonsterReal/spawnFakeMonster.
		void skip_temporary_stats(InPacket& recv)
		{
			// writeLongEncodeTemporaryMask writes four ints unconditionally
			// (PacketCreator.java:3049-3062).
			uint32_t masks[4];

			for (size_t i = 0; i < 4; i++)
				masks[i] = static_cast<uint32_t>(recv.read_int());

			// One entry per set status bit: short value, four bytes of mob skill
			// (writeMobSkillId, PacketCreator.java:142-145) or of skill id, short
			// duration - eight bytes in total.
			for (size_t i = 0; i < 4; i++)
				for (uint32_t bit = 1; bit != 0; bit <<= 1)
					if (masks[i] & bit)
						recv.skip(8);

			// Weapon and magic reflection append one counter each plus a probability
			// (PacketCreator.java:1463-1472). Both statuses are applied by mob skills
			// only, so a set bit always implies its counter (MobSkill.java:232-247).
			// Both are first-half statuses, so their bits are in the first mask word.
			bool weaponreflect = (masks[0] & 0x20000000) != 0;	// MonsterStatus.java:54
			bool magicreflect = (masks[0] & 0x40000000) != 0;	// MonsterStatus.java:55

			if (weaponreflect)
				recv.skip(4);	// wPCounter

			if (magicreflect)
				recv.skip(4);	// wMCounter

			if (weaponreflect || magicreflect)
				recv.skip(4);	// nCounterProb
		}

		// Mirrors PacketCreator.addRingLook (PacketCreator.java:2120-2144): a flag
		// byte and, only when a ring is equipped, its twenty bytes. The wire format
		// carries no ring count, so a single ring per category can be consumed.
		void skip_ring_look(InPacket& recv)
		{
			if (recv.read_byte() == 0)
				return;

			recv.skip(20);	// ringid, 0, partner ringid, 0, itemid
		}

		// Mirrors PacketCreator.addMarriageRingLook (PacketCreator.java:2146-2172).
		void skip_marriage_ring_look(InPacket& recv)
		{
			if (recv.read_byte() == 0)
				return;

			recv.skip(12);	// two chr ids and the item id
		}

		// Mirrors PacketCreator.encodeNewYearCardInfo (PacketCreator.java:2030-2042).
		void skip_newyear_card_info(InPacket& recv)
		{
			if (recv.read_byte() == 0)
				return;

			int32_t count = recv.read_int();

			for (int32_t i = 0; i < count; i++)
				recv.read_int();
		}

		// Parses the tail of SPAWN_MONSTER(236) and SPAWN_MONSTER_CONTROL(238)
		// (PacketCreator.java:1525-1538, 1560-1567, 1586-1588) and returns the
		// fade-in flag of the spawn.
		bool parse_mob_spawn_tail(InPacket& recv, int8_t& team)
		{
			int8_t effect = recv.read_byte();
			bool newspawn;

			if (effect == -3)
			{
				// Mob linked to a parent mob: it appears once the parent dies
				// (PacketCreator.java:1527-1529). No fade marker is written here.
				recv.read_int();	// parent mob oid
				newspawn = false;
			}
			else
			{
				if (effect > 0)
				{
					// Spawn effect block (PacketCreator.java:1421-1428).
					recv.read_byte();	// 0
					recv.read_short();	// 0

					if (effect == 15)
						recv.read_byte();	// 0

					// The trailing marker carries the fade-in flag
					// (PacketCreator.java:1429).
					effect = recv.read_byte();
				}

				newspawn = effect == -2;

				// spawnFakeMonster writes that marker as a short instead of a byte
				// (PacketCreator.java:1565), so one extra byte follows it.
				if (recv.length() > 5)
					recv.skip(1);
			}

			team = recv.read_byte();
			recv.skip(4);	// getItemEffect (PacketCreator.java:1538)

			return newspawn;
		}
	}

	void SpawnCharHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();

		// We don't need to spawn the player twice
		if (Stage::get().is_player(cid))
			return;

		uint8_t level = recv.read_byte();	// PacketCreator.java:1931 (v83)
		std::string name = recv.read_string();

		recv.read_string();	// guildname
		recv.read_short();	// guildlogobg
		recv.read_byte();	// guildlogobgcolor
		recv.read_short();	// guildlogo
		recv.read_byte();	// guildlogocolor

		// writeForeignBuffs (PacketCreator.java:1834-1837)
		recv.skip(8);

		bool morphed = recv.read_int() == 2;
		int32_t buffmask1 = recv.read_int();
		int16_t buffvalue = 0;

		// The buff value is written for COMBO and MORPH only, not for every set
		// buff mask bit (PacketCreator.java:1863-1868).
		if (morphed || (buffmask1 & FOREIGN_BUFF_COMBO) != 0)
			buffvalue = morphed ? recv.read_short() : recv.read_byte();

		recv.read_int(); // buffmask 2

		recv.skip(43);

		recv.read_int(); // 'mount'

		recv.skip(61);

		int16_t job = recv.read_short();
		LookEntry look = LoginParser::parse_look(recv);

		recv.read_int(); // count of 5110000 
		recv.read_int(); // 'itemeffect'
		recv.read_int(); // 'chair'

		Point<int16_t> position = recv.read_point();
		int8_t stance = recv.read_byte();

		recv.skip(3);	// fh and spawn byte (PacketCreator.java:1976-1977)

		// Pets: one block per non-null slot, followed by a terminating zero byte
		// (PacketCreator.java:1979-1984). The block is addPetInfo without the
		// showpet byte (PacketCreator.java:4426-4440).
		int8_t petslot = recv.read_byte();

		for (size_t i = 0; i < 3 && petslot == 1; i++)
		{
			recv.read_int();	// itemid
			recv.read_string();	// name
			recv.read_long();	// unique id
			recv.read_point();	// pos
			recv.read_byte();	// stance
			recv.read_short();	// fh
			recv.read_byte();	// has name tag
			recv.read_byte();	// has chat balloon

			petslot = recv.read_byte();
		}

		recv.read_int(); // mountlevel
		recv.read_int(); // mountexp
		recv.read_int(); // mounttiredness

		// Announce box: player shop (4), mini game (1 or 2) or none (0)
		// (PacketCreator.java:1993-2009 and 2180-2205)
		int8_t box = recv.read_byte();

		if (box == 4)
		{
			recv.read_int();	// shop object id
			recv.read_string();	// description
			recv.skip(5);		// 0, 0, 1, availability, 0
		}
		else if (box != 0)
		{
			recv.read_int();	// game object id
			recv.read_string();	// description
			recv.skip(5);		// password, piece type, amount, capacity, joinable
		}

		bool chalkboard = recv.read_bool();
		std::string chalktext = chalkboard ? recv.read_string() : "";

		// Tail: crush ring, friendship ring, marriage ring, new year cards, team
		// (PacketCreator.java:2020-2026)
		skip_ring_look(recv);
		skip_ring_look(recv);
		skip_marriage_ring_look(recv);
		skip_newyear_card_info(recv);

		recv.skip(2);
		int8_t team = recv.read_byte();

		Stage::get().get_chars().spawn(
			{ cid, look, level, job, name, stance, position }
		);
	}

	void RemoveCharHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();

		Stage::get().get_chars().remove(cid);
	}

	void SpawnPetHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		Optional<Char> character = Stage::get().get_character(cid);

		if (!character)
			return;

		uint8_t petindex = recv.read_byte();
		int8_t mode = recv.read_byte();

		if (mode == 1)
		{
			recv.skip(1);

			int32_t itemid = recv.read_int();
			std::string name = recv.read_string();
			int32_t uniqueid = recv.read_int();

			recv.skip(4);

			Point<int16_t> pos = recv.read_point();
			uint8_t stance = recv.read_byte();
			int32_t fhid = recv.read_int();

			character->add_pet(petindex, itemid, name, uniqueid, pos, stance, fhid);
		}
		else if (mode == 0)
		{
			bool hunger = recv.read_bool();

			character->remove_pet(petindex, hunger);
		}
	}

	void CharMovedHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		recv.skip(4);
		std::vector<Movement> movements = MovementParser::parse_movements(recv);

		Stage::get().get_chars().send_movement(cid, movements);
	}

	void UpdateCharLookHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		recv.read_byte();
		LookEntry look = LoginParser::parse_look(recv);

		// Ring looks appended by PacketCreator.updateCharLook
		// (PacketCreator.java:2591-2594)
		skip_ring_look(recv);
		skip_ring_look(recv);
		skip_marriage_ring_look(recv);
		recv.read_int();	// 0

		Stage::get().get_chars().update_look(cid, look);
	}

	void ShowForeignEffectHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		int8_t effect = recv.read_byte();

		if (effect == 4) // pet level up
		{
			// byte 0 and the pet index (PacketCreator.java:4513-4519)
			recv.read_byte();
			recv.read_byte();
		}
		else if (effect == 10) // recovery
		{
			recv.read_byte(); // 'amount'
		}
		else if (effect == 13) // card effect
		{
			Stage::get().show_character_effect(cid, CharEffect::MONSTER_CARD);
		}
		else if (effect == 16) // maker skill
		{
			// 0 on success, 1 on failure (PacketCreator.java:6231-6236)
			recv.read_int();
		}
		else if (effect == 23) // info, e.g. fishing (PacketCreator.java:6091-6097)
		{
			recv.read_string();	// path
			recv.read_int();	// 1
		}
		else if (recv.length() >= 4) // skill
		{
			int32_t skillid = recv.read_int();

			if (recv.length() == 10)
			{
				// showBuffEffect(cid, skillId, effectId, direction):
				// byte direction, byte 1, long 0 (PacketCreator.java:3459-3467)
				recv.read_byte();	// 'direction'
				recv.read_byte();
				recv.read_long();
			}
			else
			{
				// showBuffEffect(cid, skillId, skillLv, effectId, direction) and
				// showBerserk: byte 0 or 0xA9, byte skill level, byte direction
				// (PacketCreator.java:3470-3478, 3500-3508)
				recv.read_byte();
				recv.read_byte();	// skill level
				recv.read_byte();	// 'direction'
			}

			Stage::get().get_combat().show_buff(cid, skillid, effect);
		}
		else if (recv.available())
		{
			LOG(LOG_NETWORK, "[ShowForeignEffectHandler] Unhandled effect " << static_cast<int16_t>(effect)
				<< ", " << recv.length() << " bytes left unconsumed");
		}
		else
		{
			// TODO: Blank
		}
	}

	void SpawnMobHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		recv.read_byte(); // 5 if controller == null
		int32_t id = recv.read_int();

		// SPAWN_MONSTER always carries the mask of an empty temporary status
		// block, sixteen zero bytes (PacketCreator.java:1508).
		skip_temporary_stats(recv);

		Point<int16_t> position = recv.read_point();
		int8_t stance = recv.read_byte();

		recv.skip(2);	// origin fh (PacketCreator.java:1513)

		uint16_t fh = recv.read_short();

		int8_t team = 0;
		bool newspawn = parse_mob_spawn_tail(recv, team);

		Stage::get().get_mobs().spawn(
			{ oid, id, 0, stance, fh, newspawn, team, position }
		);
	}

	void KillMobHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t animation = recv.read_byte();

		// The animation is written twice (PacketCreator.java:1780-1786).
		recv.read_byte();

		// Mob::kill only implements the animations 0, 1 and 2 while the server also
		// sends 4 (monster bomb, MonsterBombHandler.java:40) and the action id of a
		// self destructing mob (MapleMap.java:1363). Without this the mob would stay
		// visible after the server removed it.
		if (animation > 2)
			animation = 1;

		Stage::get().get_mobs().remove(oid, animation);
	}

	void SpawnMobControllerHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();

		if (mode == 0)
		{
			Stage::get().get_mobs().set_control(oid, false);
		}
		else
		{
			if (recv.available())
			{
				recv.skip(1);

				int32_t id = recv.read_int();

				// The mask of the temporary status block, sixteen bytes when the
				// mob has no statuses (PacketCreator.java:1506, 1432-1473).
				skip_temporary_stats(recv);

				Point<int16_t> position = recv.read_point();
				int8_t stance = recv.read_byte();

				recv.skip(2);	// origin fh (PacketCreator.java:1513)

				uint16_t fh = recv.read_short();

				int8_t team = 0;
				bool newspawn = parse_mob_spawn_tail(recv, team);

				Stage::get().get_mobs().spawn(
					{ oid, id, mode, stance, fh, newspawn, team, position }
				);
			}
			else
			{
				// TODO: Remove monster invisibility, not used (maybe in an event script?), Check this!
			}
		}
	}

	void MobMovedHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();

		recv.read_byte();
		recv.read_byte(); // useskill
		recv.read_byte(); // skill
		recv.read_byte(); // skill 1
		recv.read_byte(); // skill 2
		recv.read_byte(); // skill 3
		recv.read_byte(); // skill 4

		Point<int16_t> position = recv.read_point();
		std::vector<Movement> movements = MovementParser::parse_movements(recv);

		Stage::get().get_mobs().send_movement(oid, position, std::move(movements));
	}

	void ShowMobHpHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t hppercent = recv.read_byte();
		uint16_t playerlevel = Stage::get().get_player().get_stats().get_stat(MapleStat::Id::LEVEL);

		Stage::get().get_mobs().send_mobhp(oid, hppercent, playerlevel);
	}

	void SpawnNpcHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int32_t id = recv.read_int();
		Point<int16_t> position = recv.read_point();
		bool flip = recv.read_bool();
		uint16_t fh = recv.read_short();

		recv.read_short(); // 'rx'
		recv.read_short(); // 'ry'
		recv.read_byte(); // 1 (PacketCreator.java:1343)

		Stage::get().get_npcs().spawn(
			{ oid, id, position, flip, fh }
		);
	}

	void SpawnNpcControllerHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();

		if (mode == 0)
		{
			Stage::get().get_npcs().remove(oid);
		}
		else
		{
			int32_t id = recv.read_int();
			Point<int16_t> position = recv.read_point();
			bool flip = recv.read_bool();
			uint16_t fh = recv.read_short();

			recv.read_short();	// 'rx'
			recv.read_short();	// 'ry'
			recv.read_bool();	// 'minimap'

			Stage::get().get_npcs().spawn(
				{ oid, id, position, flip, fh }
			);
		}
	}

	void DropLootHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();
		bool meso = recv.read_bool();
		int32_t itemid = recv.read_int();
		int32_t owner = recv.read_int();
		int8_t pickuptype = recv.read_byte();
		Point<int16_t> dropto = recv.read_point();

		recv.skip(4);

		Point<int16_t> dropfrom;

		if (mode != 2)
		{
			dropfrom = recv.read_point();

			recv.skip(2);

			Sound(Sound::Name::DROP).play();
		}
		else
		{
			dropfrom = dropto;
		}

		if (!meso)
			recv.skip(8);

		bool playerdrop = !recv.read_bool();

		Stage::get().get_drops().spawn(
			{ oid, itemid, meso, owner, dropfrom, dropto, pickuptype, mode, playerdrop }
		);
	}

	void RemoveLootHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();
		int32_t oid = recv.read_int();

		Optional<PhysicsObject> looter;

		if (mode > 1)
		{
			int32_t cid = recv.read_int();

			if (recv.length() > 0)
				recv.read_byte(); // pet
			else if (auto character = Stage::get().get_character(cid))
				looter = character->get_phobj();

			Sound(Sound::Name::PICKUP).play();
		}

		// Drop::expire handles the animations 0 (expire), 1 (remove) and 2 (pickup)
		// only, while the server also sends 4 for items taken by an area attack
		// (AbstractDealDamageHandler.java:228) and 5 when a pet loots the item
		// (Character.java:2044).
		if (mode == 4)
			mode = 1;
		else if (mode == 5)
			mode = 2;

		Stage::get().get_drops().remove(oid, mode, looter.get());
	}

	void HitReactorHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t state = recv.read_byte();
		Point<int16_t> point = recv.read_point();
		int8_t stance = recv.read_byte(); // TODO: When is this different than state?
		recv.skip(2); // TODO: Unused
		recv.skip(1); // "frame" delay but this is in the WZ file?

		Stage::get().get_reactors().trigger(oid, state);
	}

	void SpawnReactorHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int32_t rid = recv.read_int();
		int8_t state = recv.read_byte();
		Point<int16_t> point = recv.read_point();

		// fhid and an extra short, both zero (PacketCreator.java:4187-4188)
		recv.read_byte();
		recv.read_short();

		Stage::get().get_reactors().spawn(
			{ oid, rid, state, point }
		);
	}

	void RemoveReactorHandler::handle(InPacket& recv) const
	{
		int32_t oid = recv.read_int();
		int8_t state = recv.read_byte();
		Point<int16_t> point = recv.read_point();

		Stage::get().get_reactors().remove(oid, state, point);
	}
}