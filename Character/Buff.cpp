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
#include "Buff.h"

namespace ms
{
	namespace Buffstat
	{
		// Bits of the second long, from buffs (client/BuffStat.java:26-100) and from
		// debuffs (client/Disease.java:31-41). Sorted ascending, every id owns its own
		// bit: two names sharing a bit on the server (COMBO/SUMMON, PUPPET/ZOMBIFY,
		// MAP_PROTECTION/CURSE, SHADOW_CLAW/FISHABLE, ...) cost one triple in total and
		// therefore get one id here.
		const std::unordered_map<Id, uint64_t> second_codes =
		{
			{ Id::SLOW, 0x1 },
			{ Id::MORPH, 0x2 },
			{ Id::RECOVERY, 0x4 },
			{ Id::MAPLE_WARRIOR, 0x8 },
			{ Id::STANCE, 0x10 },
			{ Id::SHARP_EYES, 0x20 },
			{ Id::MANA_REFLECTION, 0x40 },
			{ Id::SEDUCE, 0x80 },
			{ Id::SHADOW_CLAW, 0x100 },
			{ Id::INFINITY_, 0x200 },
			{ Id::HOLY_SHIELD, 0x400 },
			{ Id::HAMSTRING, 0x800 },
			{ Id::BLIND, 0x1000 },
			{ Id::CONCENTRATE, 0x2000 },
			{ Id::PUPPET, 0x4000 },
			{ Id::ECHO_OF_HERO, 0x8000 },
			{ Id::MESO_UP_BY_ITEM, 0x10000 },
			{ Id::GHOST_MORPH, 0x20000 },
			{ Id::AURA, 0x40000 },
			{ Id::CONFUSE, 0x80000 },
			{ Id::ITEM_UP_BY_ITEM, 0x100000 },
			{ Id::RESPECT_PIMMUNE, 0x200000 },
			{ Id::RESPECT_MIMMUNE, 0x400000 },
			{ Id::DEFENSE_ATT, 0x800000 },
			{ Id::DEFENSE_STATE, 0x1000000 },
			{ Id::HPREC, 0x2000000 },
			{ Id::MPREC, 0x4000000 },
			{ Id::BERSERK_FURY, 0x8000000 },
			{ Id::DIVINE_BODY, 0x10000000 },
			{ Id::SPARK, 0x20000000 },
			{ Id::EXP_BUFF, 0x40000000 },
			{ Id::FINALATTACK, 0x80000000 },
			{ Id::WATK, 0x100000000 },
			{ Id::WDEF, 0x200000000 },
			{ Id::MATK, 0x400000000 },
			{ Id::MDEF, 0x800000000 },
			{ Id::ACC, 0x1000000000 },
			{ Id::AVOID, 0x2000000000 },
			{ Id::HANDS, 0x4000000000 },
			{ Id::SPEED, 0x8000000000 },
			{ Id::JUMP, 0x10000000000 },
			{ Id::MAGIC_GUARD, 0x20000000000 },
			{ Id::DARKSIGHT, 0x40000000000 },
			{ Id::BOOSTER, 0x80000000000 },
			{ Id::POWERGUARD, 0x100000000000 },
			{ Id::HYPERBODYHP, 0x200000000000 },
			{ Id::HYPERBODYMP, 0x400000000000 },
			{ Id::INVINCIBLE, 0x800000000000 },
			{ Id::SOULARROW, 0x1000000000000 },
			{ Id::STUN, 0x2000000000000 },
			{ Id::POISON, 0x4000000000000 },
			{ Id::SEAL, 0x8000000000000 },
			{ Id::DARKNESS, 0x10000000000000 },
			{ Id::COMBO, 0x20000000000000 },
			{ Id::WK_CHARGE, 0x40000000000000 },
			{ Id::DRAGONBLOOD, 0x80000000000000 },
			{ Id::HOLY_SYMBOL, 0x100000000000000 },
			{ Id::MESOUP, 0x200000000000000 },
			{ Id::SHADOWPARTNER, 0x400000000000000 },
			{ Id::PICKPOCKET, 0x800000000000000 },
			{ Id::MESOGUARD, 0x1000000000000000 },
			{ Id::EXP_INCREASE, 0x2000000000000000 },
			{ Id::WEAKEN, 0x4000000000000000 },
			{ Id::MAP_PROTECTION, 0x8000000000000000 }
		};

		// Bits of the first long, i.e. the buffstats whose BuffStat.isFirst() is true
		// (client/BuffStat.java:103-122). BuffStat.SLOW/ELEMENTAL_RESET and
		// MAGIC_SHIELD/WIND_WALK share a bit on the server, so they share an id here.
		const std::unordered_map<Id, uint64_t> first_codes =
		{
			{ Id::ELEMENTAL_RESET, 0x200000000 },
			{ Id::MAGIC_SHIELD, 0x400000000 },
			{ Id::MAGIC_RESISTANCE, 0x800000000 },
			{ Id::ARAN_COMBO, 0x1000000000 },
			{ Id::COMBO_DRAIN, 0x2000000000 },
			{ Id::COMBO_BARRIER, 0x4000000000 },
			{ Id::BODY_PRESSURE, 0x8000000000 },
			{ Id::SMART_KNOCKBACK, 0x10000000000 },
			{ Id::BERSERK, 0x20000000000 },
			{ Id::ENERGY_CHARGE, 0x4000000000000 },
			{ Id::DASH2, 0x8000000000000 },
			{ Id::DASH, 0x10000000000000 },
			{ Id::MONSTER_RIDING, 0x20000000000000 },
			{ Id::SPEED_INFUSION, 0x40000000000000 },
			{ Id::HOMING_BEACON, 0x80000000000000 }
		};

		uint64_t code(Id id, bool first)
		{
			const std::unordered_map<Id, uint64_t>& codes = first ? first_codes : second_codes;
			auto iter = codes.find(id);

			return iter == codes.end() ? 0 : iter->second;
		}

		Id by_bit(uint64_t bit, bool first)
		{
			const std::unordered_map<Id, uint64_t>& codes = first ? first_codes : second_codes;

			for (auto& iter : codes)
				if (iter.second == bit)
					return iter.first;

			return Id::NONE;
		}
	}
}