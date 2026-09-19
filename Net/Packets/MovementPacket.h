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

#include "../OutPacket.h"

namespace ms
{
	// Base class for packets which update object movements with the server
	class MovementPacket : public OutPacket
	{
	public:
		MovementPacket(OutPacket::Opcode opc) : OutPacket(opc) {}

	protected:
		// The widths have to be the ones the server reads for each command
		// (AbstractMovementPacketHandler.updatePosition,
		// AbstractMovementPacketHandler.java:159-258) and the ones MovementParser
		// reads back from a relayed blob.
		void writemovement(const Movement& movement)
		{
			write_byte(movement.command);

			switch (movement.command)
			{
			case 0:
			case 5:
			case 17:
				write_short(movement.xpos);
				write_short(movement.ypos);
				write_short(movement.lastx);
				write_short(movement.lasty);
				write_short(movement.fh);
				write_byte(movement.newstate);
				write_short(movement.duration);
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
				write_short(movement.xpos);
				write_short(movement.ypos);
				write_byte(movement.newstate);
				write_short(movement.duration);
				break;
			case 3:
			case 4:
			case 7:
			case 8:
			case 9:
				write_short(movement.xpos);
				write_short(movement.ypos);
				skip(4);	// xwobble, ywobble
				write_byte(movement.newstate);
				break;
			case 11:
				write_short(movement.xpos);
				write_short(movement.ypos);
				skip(2);
				write_byte(movement.newstate);
				write_short(movement.duration);
				break;
			case 15:
				write_short(movement.xpos);
				write_short(movement.ypos);
				write_short(movement.lastx);
				write_short(movement.lasty);
				skip(2);
				write_short(movement.fh);
				write_byte(movement.newstate);
				write_short(movement.duration);
				break;
			case 10:
				skip(1);	// change equip
				break;
			case 14:
				skip(9);	// jump down
				break;
			case 21:
				skip(3);
				break;
			default:
				// The server drops movement packets with an unknown command
				// (AbstractMovementPacketHandler.java:255-257).
				break;
			}
		}
	};
}