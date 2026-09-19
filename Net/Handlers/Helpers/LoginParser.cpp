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
#include "LoginParser.h"

#include "../../Session.h"

namespace ms
{
	Account LoginParser::parse_account(InPacket& recv)
	{
		Account account;

		recv.skip_short();

		account.accid = recv.read_int();
		account.female = recv.read_byte();
		account.admin = recv.read_bool();

		recv.skip_byte(); // Admin
		recv.skip_byte(); // Country Code

		account.name = recv.read_string();

		recv.skip_byte();

		account.muted = recv.read_bool();

		recv.skip_long(); // muted until
		recv.skip_long(); // creation date

		recv.skip_int(); // Remove "Select the world you want to play in"

		account.pin = recv.read_bool(); // 0 - Enabled, 1 - Disabled
		account.pic = recv.read_byte(); // 0 - Register, 1 - Ask, 2 - Disabled

		return account;
	}

	World LoginParser::parse_world(InPacket& recv)
	{
		int8_t wid = recv.read_byte();

		if (wid == -1)
			return { {}, {}, {}, 0, 0, wid };

		std::string name = recv.read_string();
		uint8_t flag = recv.read_byte();
		std::string event_message = recv.read_string();

		recv.skip(5);

		std::vector<int32_t> channelload;
		uint8_t channelload_size = recv.read_byte();

		for (uint8_t i = 0; i < channelload_size; ++i)
		{
			recv.skip_string(); // channel name

			channelload.push_back(recv.read_int());

			recv.skip_byte(); // world id
			recv.skip_byte(); // channel id
			recv.skip_bool(); // adult channel
		}

		recv.skip_short();

		return { name, event_message, channelload, channelload_size, flag, wid };
	}

	CharEntry LoginParser::parse_charentry(InPacket& recv)
	{
		int32_t cid = recv.read_int();
		StatsEntry stats = parse_stats(recv);
		LookEntry look = parse_look(recv);

		recv.read_bool(); // 'rankinfo' bool

		if (recv.read_bool())
		{
			int32_t currank = recv.read_int();
			int32_t rankmv = recv.read_int();
			int32_t curjobrank = recv.read_int();
			int32_t jobrankmv = recv.read_int();
			int8_t rankmc = (rankmv > 0) ? '+' : (rankmv < 0) ? '-' : '=';
			int8_t jobrankmc = (jobrankmv > 0) ? '+' : (jobrankmv < 0) ? '-' : '=';

			stats.rank = std::make_pair(currank, rankmc);
			stats.jobrank = std::make_pair(curjobrank, jobrankmc);
		}

		return { stats, look, cid };
	}

	StatsEntry LoginParser::parse_stats(InPacket& recv)
	{
		// TODO: This is similar to CashShopParser.cpp, try and merge these.
		StatsEntry statsentry;

		statsentry.name = recv.read_padded_string(13);
		statsentry.female = recv.read_bool();

		recv.read_byte();	// skin
		recv.read_int();	// face
		recv.read_int();	// hair

		for (size_t i = 0; i < 3; i++)
			statsentry.petids.push_back(recv.read_long());

		// The level is a single byte here; reading a short eats the job's low byte
		statsentry.stats[MapleStat::Id::LEVEL] = recv.read_byte();
		statsentry.stats[MapleStat::Id::JOB] = recv.read_short();
		statsentry.stats[MapleStat::Id::STR] = recv.read_short();
		statsentry.stats[MapleStat::Id::DEX] = recv.read_short();
		statsentry.stats[MapleStat::Id::INT] = recv.read_short();
		statsentry.stats[MapleStat::Id::LUK] = recv.read_short();
		statsentry.stats[MapleStat::Id::HP] = recv.read_short();
		statsentry.stats[MapleStat::Id::MAXHP] = recv.read_short();
		statsentry.stats[MapleStat::Id::MP] = recv.read_short();
		statsentry.stats[MapleStat::Id::MAXMP] = recv.read_short();
		statsentry.stats[MapleStat::Id::AP] = recv.read_short();

		// The server writes a variable-length per-book SP table instead of a short when the
		// job stores its SP per skill book: addCharStats (PacketCreator.java:202-205)
		//		if (GameConstants.hasSPTable(chr.getJob())) {
		//			addRemainingSkillInfo(p, chr);
		//		} else {
		//			p.writeShort(chr.getRemainingSp()); // remaining sp
		//		}
		if (has_sp_table(statsentry.stats[MapleStat::Id::JOB]))
			statsentry.stats[MapleStat::Id::SP] = parse_remaining_skill_info(recv);
		else
			statsentry.stats[MapleStat::Id::SP] = recv.read_short();
		statsentry.exp = recv.read_int();
		statsentry.stats[MapleStat::Id::FAME] = recv.read_short();

		recv.skip(4); // gachaexp

		statsentry.mapid = recv.read_int();
		statsentry.portal = recv.read_byte();

		recv.skip(4); // timestamp

		return statsentry;
	}

	bool LoginParser::has_sp_table(uint16_t job)
	{
		// GameConstants.hasSPTable (GameConstants.java:604-620) returns true for EVAN and
		// EVAN1..EVAN10.
		switch (job)
		{
		case LoginParser::EVAN:
		case LoginParser::EVAN1:
		case LoginParser::EVAN2:
		case LoginParser::EVAN3:
		case LoginParser::EVAN4:
		case LoginParser::EVAN5:
		case LoginParser::EVAN6:
		case LoginParser::EVAN7:
		case LoginParser::EVAN8:
		case LoginParser::EVAN9:
		case LoginParser::EVAN10:
			return true;
		default:
			return false;
		}
	}

	uint16_t LoginParser::parse_remaining_skill_info(InPacket& recv)
	{
		// addRemainingSkillInfo (PacketCreator.java:155-171) writes
		//		p.writeByte(effectiveLength);
		//		for (int i = 0; i < remainingSp.length; i++) {
		//			if (remainingSp[i] > 0) {
		//				p.writeByte(i + 1);
		//				p.writeByte(remainingSp[i]);
		//			}
		//		}
		// so the block is 1 + 2 * count bytes, count <= 10 (Character.java:6419 allocates ten
		// books). A StatsEntry has a single SP slot and the skill window renders a single
		// counter (UISkillBook::change_sp), so the per-book values are summed: every remaining
		// point stays visible and spendable instead of being dropped with the books.
		uint8_t count = static_cast<uint8_t>(recv.read_byte());
		uint16_t total = 0;

		for (uint8_t i = 0; i < count; i++)
		{
			recv.read_byte(); // SP book index (the server writes i + 1)
			uint16_t book_sp = static_cast<uint8_t>(recv.read_byte()); // SP held by that book
			total = static_cast<uint16_t>(total + book_sp);
		}

		return total;
	}

	LookEntry LoginParser::parse_look(InPacket& recv)
	{
		LookEntry look;

		look.female = recv.read_bool();
		look.skin = recv.read_byte();
		look.faceid = recv.read_int();

		recv.read_bool(); // megaphone

		look.hairid = recv.read_int();

		uint8_t eqslot = recv.read_byte();

		while (eqslot != 0xFF)
		{
			look.equips[eqslot] = recv.read_int();
			eqslot = recv.read_byte();
		}

		uint8_t mskeqslot = recv.read_byte();

		while (mskeqslot != 0xFF)
		{
			look.maskedequips[mskeqslot] = recv.read_int();
			mskeqslot = recv.read_byte();
		}

		look.maskedequips[-111] = recv.read_int();

		for (uint8_t i = 0; i < 3; i++)
			look.petids.push_back(recv.read_int());

		return look;
	}

	void LoginParser::parse_login(InPacket& recv)
	{
		recv.skip_byte();

		// Read the IPv4 address in a string
		std::string addrstr;

		for (size_t i = 0; i < 4; i++)
		{
			uint8_t num = static_cast<uint8_t>(recv.read_byte());
			addrstr.append(std::to_string(num));

			if (i < 3)
				addrstr.push_back('.');
		}

		// Read the port address in a string
		// getServerIP writes the port as an unsigned 16-bit little-endian value
		// (PacketCreator.java:849 `p.writeShort(port);`), so it has to be read back unsigned:
		// a port above 32767 would otherwise turn negative and break the address string.
		std::string portstr = std::to_string(static_cast<uint16_t>(recv.read_short()));

		// Attempt to reconnect to the server
		Session::get().reconnect(addrstr.c_str(), portstr.c_str());
	}
}