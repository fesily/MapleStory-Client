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
#pragma once

#include "InPacket.h"

#include <cstdint>
#include <string>

namespace ms
{
	// Layout parsers for opcodes the server sends but the client does not act on.
	//
	// Two purposes: the wire format of those opcodes stays verified (a parser reads
	// exactly the bytes the server writes, so a server-side change shows up as a
	// parse error instead of silence), and the log states that the capability
	// itself is not wired up. The parsers live in Net/Handlers/Unsupported/, one
	// table per slice of the opcode range.
	namespace Unsupported
	{
		// Reads the payload of one opcode
		using Parser = void (*)(InPacket& recv);

		// Register the parser of an opcode, called by the slice tables
		void add(uint16_t opcode, Parser parser);

		// Parse an opcode which has no handler and log the outcome.
		// Returns false when no layout is known for the opcode.
		bool forward(uint16_t opcode, const std::string& name, InPacket& recv);

		// Registration entry points, one per table file
		void register_packets1();
		void register_packets2();
		void register_packets3();
		void register_packets4();
		void register_packets5();
		void register_packets6();
		void register_packets7();
		void register_packets8();
	}
}