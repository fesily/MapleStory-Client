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

#include "../../InPacket.h"
#include "../../Login.h"

namespace ms
{
	namespace LoginParser
	{
		Account parse_account(InPacket& recv);
		World parse_world(InPacket& recv);
		CharEntry parse_charentry(InPacket& recv);
		StatsEntry parse_stats(InPacket& recv);
		// Job ids of the Evan family, the only jobs whose remaining SP is stored per skill
		// book. Mirrors Job.java:62-66.
		enum Jobs : uint16_t
		{
			EVAN = 2001,
			EVAN1 = 2200,
			EVAN2 = 2210,
			EVAN3 = 2211,
			EVAN4 = 2212,
			EVAN5 = 2213,
			EVAN6 = 2214,
			EVAN7 = 2215,
			EVAN8 = 2216,
			EVAN9 = 2217,
			EVAN10 = 2218
		};
		// True for the jobs whose remaining SP is stored per skill book instead of in a single
		// value. Mirrors GameConstants.hasSPTable (GameConstants.java:604-620).
		bool has_sp_table(uint16_t job);
		// Reads the per-book SP table written by PacketCreator.addRemainingSkillInfo
		// (PacketCreator.java:155-171) and returns the SP summed over all its books.
		uint16_t parse_remaining_skill_info(InPacket& recv);
		LookEntry parse_look(InPacket& recv);
		void parse_login(InPacket& recv);
	}
}