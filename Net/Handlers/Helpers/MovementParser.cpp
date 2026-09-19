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
#include "MovementParser.h"

#include "../../../MapleStory.h"

#include <iostream>

namespace ms
{
	std::vector<Movement> MovementParser::parse_movements(InPacket& recv)
	{
		std::vector<Movement> movements;
		uint8_t length = recv.read_byte();

		for (uint8_t i = 0; i < length; ++i)
		{
			Movement fragment;
			fragment.command = recv.read_byte();

			// The widths below are the ones the server consumes in
			// AbstractMovementPacketHandler.updatePosition
			// (AbstractMovementPacketHandler.java:159-258). The server relays exactly
			// the bytes it read, so a fragment of a different width would shift every
			// following fragment of the blob.
			switch (fragment.command)
			{
			case 0:
			case 5:
			case 17:
				fragment.type = Movement::ABSOLUTE;
				fragment.xpos = recv.read_short();
				fragment.ypos = recv.read_short();
				fragment.lastx = recv.read_short();
				fragment.lasty = recv.read_short();
				fragment.fh = recv.read_short();
				fragment.newstate = recv.read_byte();
				fragment.duration = recv.read_short();
				break;
			case 1:
			case 2:
			case 6:
			case 12:
			case 13:
			case 16:
			case 18:
			case 19:
			case 20:
			case 22:
				fragment.type = Movement::RELATIVE;
				fragment.xpos = recv.read_short();
				fragment.ypos = recv.read_short();
				fragment.newstate = recv.read_byte();
				fragment.duration = recv.read_short();
				break;
			case 3:
			case 4:
			case 7:
			case 8:
			case 9:
				// Teleport and dash-like moves carry the position they end on
				// (AbstractMovementPacketHandler.java:265-286)
				fragment.type = Movement::ABSOLUTE;
				fragment.xpos = recv.read_short();
				fragment.ypos = recv.read_short();
				fragment.lastx = fragment.xpos;
				fragment.lasty = fragment.ypos;
				recv.skip(4);	// xwobble, ywobble
				fragment.newstate = recv.read_byte();
				fragment.duration = 0;
				break;
			case 11:
				fragment.type = Movement::CHAIR;
				fragment.xpos = recv.read_short();
				fragment.ypos = recv.read_short();
				recv.skip(2);
				fragment.newstate = recv.read_byte();
				fragment.duration = recv.read_short();
				break;
			case 15:
				fragment.type = Movement::JUMPDOWN;
				fragment.xpos = recv.read_short();
				fragment.ypos = recv.read_short();
				fragment.lastx = recv.read_short();
				fragment.lasty = recv.read_short();
				recv.skip(2);
				fragment.fh = recv.read_short();
				fragment.newstate = recv.read_byte();
				fragment.duration = recv.read_short();
				break;
			case 10:
				fragment.type = Movement::NONE;
				// Change equip
				recv.skip(1);
				break;
			case 14:
				fragment.type = Movement::NONE;
				recv.skip(9);	// jump down
				break;
			case 21:
				fragment.type = Movement::NONE;
				recv.skip(3);
				break;
			default:
				// The server drops movement packets with an unknown command
				// (AbstractMovementPacketHandler.java:255-257), so a relayed blob
				// cannot contain one.
				LOG(LOG_NETWORK, "[MovementParser] Unknown movement command "
					<< static_cast<uint16_t>(fragment.command) << ", "
					<< recv.length() << " bytes left unparsed");

				movements.push_back(fragment);
				return movements;
			}

			movements.push_back(fragment);
		}

		return movements;
	}
}