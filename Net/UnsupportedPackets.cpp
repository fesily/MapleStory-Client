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
#include "UnsupportedPackets.h"

#include "../MapleStory.h"

#include <iostream>

namespace ms
{
	namespace Unsupported
	{
		namespace
		{
			// Same bound as the handler array of PacketSwitch (its largest opcode is 0x1000)
			const size_t MAX_OPCODE = 0x1001;

			Parser parsers[MAX_OPCODE] = {};

			// The slice tables register themselves on the first packet which
			// has no handler, so no static initialisation order is involved
			void register_all()
			{
				static bool registered = false;

				if (registered)
					return;

				registered = true;

				register_packets1();
				register_packets2();
				register_packets3();
				register_packets4();
				register_packets5();
				register_packets6();
				register_packets7();
				register_packets8();
			}
		}

		void add(uint16_t opcode, Parser parser)
		{
			if (opcode < MAX_OPCODE)
				parsers[opcode] = parser;
		}

		bool forward(uint16_t opcode, const std::string& name, InPacket& recv)
		{
			register_all();

			if (opcode >= MAX_OPCODE || !parsers[opcode])
				return false;

			size_t available = recv.length();

			try
			{
				parsers[opcode](recv);
			}
			catch (const PacketError& err)
			{
				// The layout mirrored here no longer matches the server. The packet is
				// unused anyway, so the remaining bytes are dropped after saying so.
				LOG(LOG_DEBUG, "[Unsupported] Opcode [{}] payload parse failed: {} (capability not implemented)",
					name, err.what());

				return true;
			}

			size_t read = available - recv.length();

			LOG(LOG_DEBUG, "[Unsupported] Opcode [{}] payload parsed ({} of {} bytes, {} left), capability not implemented",
				name, read, available, recv.length());

			return true;
		}
	}
}