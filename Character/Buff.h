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
#pragma once

#include <cstdint>
#include <unordered_map>

namespace ms
{
	namespace Buffstat
	{
		// One id per mask bit. The server writes one (short value, int skillid, int
		// duration) triple for every bit it sets in GIVE_BUFF (PacketCreator.giveBuff,
		// PacketCreator.java:2803-2822), so two ids must never own the same bit: the
		// second id would make the client read a triple the server never wrote.
		// Bits are from the server's BuffStat enum (client/BuffStat.java:26-122) and,
		// for the second mask, from its Disease enum (client/Disease.java:31-41), whose
		// values are written into the same long (Disease.isFirst() is always false).
		enum Id
		{
			NONE,

			// Second mask.
			SLOW,				// 0x1		Disease.SLOW (BuffStat.SLOW is the first mask)
			MORPH,				// 0x2		BuffStat.java:26
			RECOVERY,			// 0x4
			MAPLE_WARRIOR,		// 0x8
			STANCE,				// 0x10
			SHARP_EYES,			// 0x20
			MANA_REFLECTION,	// 0x40
			SEDUCE,				// 0x80		Disease.SEDUCE
			SHADOW_CLAW,		// 0x100	also Disease.FISHABLE
			INFINITY_,			// 0x200
			HOLY_SHIELD,		// 0x400
			HAMSTRING,			// 0x800
			BLIND,				// 0x1000
			CONCENTRATE,		// 0x2000
			PUPPET,				// 0x4000	also Disease.ZOMBIFY
			ECHO_OF_HERO,		// 0x8000
			MESO_UP_BY_ITEM,	// 0x10000
			GHOST_MORPH,		// 0x20000
			AURA,				// 0x40000
			CONFUSE,			// 0x80000	also Disease.CONFUSE
			ITEM_UP_BY_ITEM,	// 0x100000	also BuffStat.COUPON_EXP1
			RESPECT_PIMMUNE,	// 0x200000	also BuffStat.COUPON_EXP2
			RESPECT_MIMMUNE,	// 0x400000	also BuffStat.COUPON_EXP3/4
			DEFENSE_ATT,		// 0x800000	also BuffStat.COUPON_DRP1
			DEFENSE_STATE,		// 0x1000000	also BuffStat.COUPON_DRP2/3
			HPREC,				// 0x2000000
			MPREC,				// 0x4000000
			BERSERK_FURY,		// 0x8000000
			DIVINE_BODY,		// 0x10000000
			SPARK,				// 0x20000000
			EXP_BUFF,			// 0x40000000	also BuffStat.MAP_CHAIR
			FINALATTACK,		// 0x80000000
			WATK,				// 0x100000000
			WDEF,				// 0x200000000
			MATK,				// 0x400000000
			MDEF,				// 0x800000000
			ACC,				// 0x1000000000
			AVOID,				// 0x2000000000
			HANDS,				// 0x4000000000
			SPEED,				// 0x8000000000
			JUMP,				// 0x10000000000
			MAGIC_GUARD,		// 0x20000000000
			DARKSIGHT,			// 0x40000000000
			BOOSTER,			// 0x80000000000
			POWERGUARD,			// 0x100000000000
			HYPERBODYHP,		// 0x200000000000
			HYPERBODYMP,		// 0x400000000000
			INVINCIBLE,			// 0x800000000000
			SOULARROW,			// 0x1000000000000
			STUN,				// 0x2000000000000		Disease.STUN
			POISON,				// 0x4000000000000		Disease.POISON
			SEAL,				// 0x8000000000000		Disease.SEAL
			DARKNESS,			// 0x10000000000000	Disease.DARKNESS
			COMBO,				// 0x20000000000000	BuffStat.SUMMON shares this bit
			WK_CHARGE,			// 0x40000000000000
			DRAGONBLOOD,		// 0x80000000000000
			HOLY_SYMBOL,		// 0x100000000000000
			MESOUP,				// 0x200000000000000
			SHADOWPARTNER,		// 0x400000000000000
			PICKPOCKET,			// 0x800000000000000
			MESOGUARD,			// 0x1000000000000000
			EXP_INCREASE,		// 0x2000000000000000
			WEAKEN,				// 0x4000000000000000	Disease.WEAKEN
			MAP_PROTECTION,		// 0x8000000000000000	also Disease.CURSE

			// First mask (BuffStat values with isFirst set, BuffStat.java:103-122).
			ELEMENTAL_RESET,	// 0x200000000		also BuffStat.SLOW
			MAGIC_SHIELD,		// 0x400000000		also BuffStat.WIND_WALK
			MAGIC_RESISTANCE,	// 0x800000000
			ARAN_COMBO,			// 0x1000000000
			COMBO_DRAIN,		// 0x2000000000
			COMBO_BARRIER,		// 0x4000000000
			BODY_PRESSURE,		// 0x8000000000
			SMART_KNOCKBACK,	// 0x10000000000
			BERSERK,			// 0x20000000000
			ENERGY_CHARGE,		// 0x4000000000000
			DASH2,				// 0x8000000000000
			DASH,				// 0x10000000000000
			MONSTER_RIDING,		// 0x20000000000000
			SPEED_INFUSION,		// 0x40000000000000
			HOMING_BEACON,		// 0x80000000000000
			LENGTH
		};

		extern const std::unordered_map<Id, uint64_t> first_codes;
		extern const std::unordered_map<Id, uint64_t> second_codes;

		// Return the bit an id owns in the given mask, 0 when it owns none.
		uint64_t code(Id id, bool first);
		// Return the id owning a single set bit of the given mask, NONE when the
		// client has no id for it. Such a bit still costs a triple: the caller has
		// to consume the payload of every set bit, understood or not.
		Id by_bit(uint64_t bit, bool first);
	}

	struct Buff
	{
		Buffstat::Id stat;
		int16_t value;
		int32_t skillid;
		int32_t duration;

		constexpr Buff(Buffstat::Id stat, int16_t value, int32_t skillid, int32_t duration) : stat(stat), value(value), skillid(skillid), duration(duration) {}
		constexpr Buff() : Buff(Buffstat::Id::NONE, 0, 0, 0) {}
	};
}