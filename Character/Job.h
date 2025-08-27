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

#include "EquipStat.h"

#include "Inventory/Weapon.h"
#include <string>
namespace ms
{
	class Job
	{
	public:
		// enum JobId
		// {
		// 	BEGINNER = 0,
		// 	WARRIOR = 100,
		// 	FIGHTER = 110,
		// 	CRUSADER = 111,
		// 	HERO = 112,
		// 	PAGE = 120,
		// 	WHITEKNIGHT = 121,
		// 	PALADIN = 122,
		// 	SPEARMAN = 130,
		// 	DRAGONKNIGHT = 131,
		// 	DARKKNIGHT = 132,

		// 	MAGICIAN = 200,
		// 	FP_WIZARD = 210,
		// 	FP_MAGE = 211,
		// 	FP_ARCHMAGE = 212,
		// 	IL_WIZARD = 220,
		// 	IL_MAGE = 221,
		// 	IL_ARCHMAGE = 222,
		// 	CLERIC = 230,
		// 	PRIEST = 231,
		// 	BISHOP = 232,

		// 	BOWMAN = 300,
		// 	HUNTER = 310,
		// 	RANGER = 311,
		// 	BOWMASTER = 312,
		// 	CROSSBOWMAN = 320,
		// 	SNIPER = 321,
		// 	MARKSMAN = 322,

		// 	THIEF = 400,
		// 	ASSASSIN = 410,
		// 	HERMIT = 411,
		// 	NIGHTLORD = 412,
		// 	BANDIT = 420,
		// 	CHIEFBANDIT = 421,
		// 	SHADOWER = 422,

		// 	PIRATE = 500,
		// 	BRAWLER = 510,
		// 	MARAUDER = 511,
		// 	BUCCANEER = 512,
		// 	GUNSLINGER = 520,
		// 	OUTLAW = 521,
		// 	CORSAIR = 522,

		// 	MAPLELEAF_BRIGADIER = 800,
		// 	GM = 900,
		// 	SUPERGM = 910,

		// 	NOBLESSE = 1000,
		// 	DAWNWARRIOR1 = 1100,
		// 	DAWNWARRIOR2 = 1110,
		// 	DAWNWARRIOR3 = 1111,
		// 	DAWNWARRIOR4 = 1112,
		// 	BLAZEWIZARD1 = 1200,
		// 	BLAZEWIZARD2 = 1210,
		// 	BLAZEWIZARD3 = 1211,
		// 	BLAZEWIZARD4 = 1212,
		// 	WINDARCHER1 = 1300,
		// 	WINDARCHER2 = 1310,
		// 	WINDARCHER3 = 1311,
		// 	WINDARCHER4 = 1312,
		// 	NIGHTWALKER1 = 1400,
		// 	NIGHTWALKER2 = 1410,
		// 	NIGHTWALKER3 = 1411,
		// 	NIGHTWALKER4 = 1412,
		// 	THUNDERBREAKER1 = 1500,
		// 	THUNDERBREAKER2 = 1510,
		// 	THUNDERBREAKER3 = 1511,
		// 	THUNDERBREAKER4 = 1512,

		// 	LEGEND = 2000,
		// 	EVAN = 2001,
		// 	ARAN1 = 2100,
		// 	ARAN2 = 2110,
		// 	ARAN3 = 2111,
		// 	ARAN4 = 2112,

		// 	EVAN1 = 2200,
		// 	EVAN2 = 2210,
		// 	EVAN3 = 2211,
		// 	EVAN4 = 2212,
		// 	EVAN5 = 2213,
		// 	EVAN6 = 2214,
		// 	EVAN7 = 2215,
		// 	EVAN8 = 2216,
		// 	EVAN9 = 2217,
		// 	EVAN10 = 2218
		// };

		enum Level : uint16_t
		{
			BEGINNER,
			FIRST,
			SECOND,
			THIRD,
			FOURTH
		};

		static Level get_next_level(Level level)
		{
			switch (level)
			{
			case BEGINNER:
				return FIRST;
			case FIRST:
				return SECOND;
			case SECOND:
				return THIRD;
			default:
				return FOURTH;
			}
		}

		Job(uint16_t id);
		Job();

		void change_job(uint16_t id);
		bool is_sub_job(uint16_t subid) const;
		bool can_use(int32_t skill_id) const;
		uint16_t get_id() const;
		uint16_t get_subjob(Level level) const;
		Level get_level() const;
		const std::string &get_name() const;
		EquipStat::Id get_primary(Weapon::Type weapontype) const;
		EquipStat::Id get_secondary(Weapon::Type weapontype) const;

	private:
		std::string get_name(uint16_t id) const;

		std::string name;
		uint16_t id;
		Level level;
	};
}