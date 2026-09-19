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
#include "PlayerHandlers.h"

#include "Helpers/LoginParser.h"

#include "../../MapleStory.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"

#include "../../IO/UITypes/UIBuffList.h"
#include "../../IO/UITypes/UICashShop.h"
#include "../../IO/UITypes/UISkillBook.h"
#include "../../IO/UITypes/UIStatsInfo.h"

namespace ms
{
	namespace
	{
		// Skill ids the pirate layout distinguishes (Buccaneer.java:35,
		// ThunderBreaker.java:46, Corsair.java:37).
		constexpr int32_t SPEED_INFUSION_BUCCANEER = 5121009;
		constexpr int32_t SPEED_INFUSION_THUNDERBREAKER = 15111005;
		constexpr int32_t HEROS_WILL_CORSAIR = 5221010;

		void apply_buff(Buffstat::Id bs, int16_t value, int32_t skillid, int32_t duration)
		{
			Stage::get().get_player().give_buff({ bs, value, skillid, duration });

			if (auto bufflist = UI::get().get_element<UIBuffList>())
				bufflist->add_buff(skillid, duration);
		}

		// Dash and speed infusion are the only buffs sent with the body of
		// PacketCreator.givePirateBuff instead of PacketCreator.giveBuff
		// (StatEffect.java:669-674 build those statups, 1283-1284 and 1347-1354 are
		// their only senders), so their bits identify that layout.
		bool is_pirate_layout(uint64_t firstmask, uint64_t secondmask)
		{
			uint64_t pirate = Buffstat::code(Buffstat::Id::DASH2, true)
				| Buffstat::code(Buffstat::Id::DASH, true)
				| Buffstat::code(Buffstat::Id::SPEED_INFUSION, true);

			return firstmask != 0 && secondmask == 0 && (firstmask & ~pirate) == 0;
		}
	}

	void ChangeChannelHandler::handle(InPacket& recv) const
	{
		LoginParser::parse_login(recv);

		auto cashshop = UI::get().get_element<UICashShop>();

		if (cashshop)
			cashshop->exit_cashshop();
	}

	void ChangeStatsHandler::handle(InPacket& recv) const
	{
		recv.read_bool(); // 'itemreaction'
		int32_t updatemask = recv.read_int();

		bool recalculate = false;

		for (auto iter : MapleStat::codes)
			if (updatemask & iter.second)
				recalculate |= handle_stat(iter.first, recv);

		if (recalculate)
			Stage::get().get_player().recalc_stats(false);

		UI::get().enable();
	}

	bool ChangeStatsHandler::handle_stat(MapleStat::Id stat, InPacket& recv) const
	{
		Player& player = Stage::get().get_player();

		bool recalculate = false;

		switch (stat)
		{
		case MapleStat::Id::SKIN:
			// Stat.SKIN is 0x1, the first branch of the server's width chain, and
			// is written as a single byte (PacketCreator.java:1013-1014).
			player.change_look(stat, recv.read_byte());
			break;
		case MapleStat::Id::FACE:
		case MapleStat::Id::HAIR:
			// 0x2 and 0x4 are written as an int (PacketCreator.java:1015-1016).
			player.change_look(stat, recv.read_int());
			break;
		case MapleStat::Id::LEVEL:
			// 0x10 is written as a single byte (PacketCreator.java:1017-1018).
			player.change_level(recv.read_byte());
			break;
		case MapleStat::Id::JOB:
			player.change_job(recv.read_short());
			break;
		case MapleStat::Id::EXP:
			player.get_stats().set_exp(recv.read_int());
			break;
		case MapleStat::Id::MESO:
			player.get_inventory().set_meso(recv.read_int());
			break;
		case MapleStat::Id::SP:
			// 0x8000: for jobs with a skill book table (the Evan line) the server
			// writes a byte per book that still has SP instead of a short
			// (PacketCreator.java:1019-1021, addRemainingSkillInfo 155-172).
			if (LoginParser::has_sp_table(player.get_stats().get_stat(MapleStat::Id::JOB)))
				player.get_stats().set_stat(stat, LoginParser::parse_remaining_skill_info(recv));
			else
				player.get_stats().set_stat(stat, recv.read_short());

			recalculate = true;
			break;
		case MapleStat::Id::PET:
		case MapleStat::Id::GACHAEXP:
			// 0x180008 and 0x200000 take the int branch of the server's width chain
			// (PacketCreator.java:1029-1030). The pet ids of PacketCreator
			// petStatUpdate (4545-4562) follow where the value goes and are dropped,
			// as CharStats has no setter for them.
			player.get_stats().set_stat(stat, static_cast<uint16_t>(recv.read_int()));
			recalculate = true;
			break;
		default:
			// All remaining stats are below 0xFFFF or equal 0x20000 and are written
			// as a short (PacketCreator.java:1025-1028).
			player.get_stats().set_stat(stat, recv.read_short());
			recalculate = true;
			break;
		}

		bool update_statsinfo = need_statsinfo_update(stat);

		if (update_statsinfo && !recalculate)
			if (auto statsinfo = UI::get().get_element<UIStatsInfo>())
				statsinfo->update_stat(stat);

		bool update_skillbook = need_skillbook_update(stat);

		if (update_skillbook)
		{
			int16_t value = player.get_stats().get_stat(stat);

			if (auto skillbook = UI::get().get_element<UISkillBook>())
				skillbook->update_stat(stat, value);
		}

		return recalculate;
	}

	bool ChangeStatsHandler::need_statsinfo_update(MapleStat::Id stat) const
	{
		switch (stat)
		{
		case MapleStat::Id::JOB:
		case MapleStat::Id::STR:
		case MapleStat::Id::DEX:
		case MapleStat::Id::INT:
		case MapleStat::Id::LUK:
		case MapleStat::Id::HP:
		case MapleStat::Id::MAXHP:
		case MapleStat::Id::MP:
		case MapleStat::Id::MAXMP:
		case MapleStat::Id::AP:
			return true;
		default:
			return false;
		}
	}

	bool ChangeStatsHandler::need_skillbook_update(MapleStat::Id stat) const
	{
		switch (stat)
		{
		case MapleStat::Id::JOB:
		case MapleStat::Id::SP:
			return true;
		default:
			return false;
		}
	}

	void BuffHandler::handle(InPacket& recv) const
	{
		uint64_t firstmask = recv.read_long();
		uint64_t secondmask = recv.read_long();

		// Walk the bits of the two masks instead of iterating the lookup tables: the
		// server sets one bit per buffed stat and writes one value per set bit
		// (PacketCreator.java:2806-2813), in the order the server collected them, so
		// counting the set bits is what keeps the packet in sync. Iterating a hash
		// map would also visit two ids that share a bit twice.
		for (uint8_t i = 0; i < 2; i++)
		{
			bool first = i == 0;
			uint64_t mask = first ? firstmask : secondmask;

			for (uint8_t bitpos = 0; bitpos < 64; bitpos++)
			{
				uint64_t bit = static_cast<uint64_t>(1) << bitpos;

				if (!(mask & bit))
					continue;

				Buffstat::Id bs = Buffstat::by_bit(bit, first);

				// Every set bit costs a value, even when the client has no name
				// for the buff: the server wrote one for it.
				if (bs == Buffstat::Id::NONE)
					LOG(LOG_NETWORK, "Unknown buff bit in " << (first ? "first" : "second") << " mask: [" << bit << "]");

				handle_buff(recv, bs);
			}
		}

		Stage::get().get_player().recalc_stats(false);
	}

	void ApplyBuffHandler::handle(InPacket& recv) const
	{
		uint64_t firstmask = static_cast<uint64_t>(recv.inspect_long());
		uint64_t secondmask = static_cast<uint64_t>(recv.inspect_long());

		if (!is_pirate_layout(firstmask, secondmask))
		{
			BuffHandler::handle(recv);
			return;
		}

		recv.skip_long();
		recv.skip_long();
		recv.skip_short(); // written before the statups (PacketCreator.java:5420)

		for (uint8_t bitpos = 0; bitpos < 64; bitpos++)
		{
			uint64_t bit = static_cast<uint64_t>(1) << bitpos;

			if (!(firstmask & bit))
				continue;

			// Int value, int buffid, a gap of five bytes (ten for speed infusion)
			// and a short duration (PacketCreator.java:5422-5425).
			int32_t value = recv.read_int();
			int32_t skillid = recv.read_int();
			bool infusion = skillid == SPEED_INFUSION_BUCCANEER
				|| skillid == SPEED_INFUSION_THUNDERBREAKER
				|| skillid == HEROS_WILL_CORSAIR;

			recv.skip(infusion ? 10 : 5);

			int16_t duration = recv.read_short();

			apply_buff(Buffstat::by_bit(bit, true), static_cast<int16_t>(value), skillid, duration);
		}

		recv.skip(3); // written after the statups (PacketCreator.java:5427)

		Stage::get().get_player().recalc_stats(false);
	}

	void ApplyBuffHandler::handle_buff(InPacket& recv, Buffstat::Id bs) const
	{
		// Short value, int skillid, int duration (PacketCreator.java:2810-2813).
		int16_t value = recv.read_short();
		int32_t skillid = recv.read_int();
		int32_t duration = recv.read_int();

		apply_buff(bs, value, skillid, duration);
	}

	void CancelBuffHandler::handle_buff(InPacket&, Buffstat::Id bs) const
	{
		Stage::get().get_player().cancel_buff(bs);
	}

	void RecalculateStatsHandler::handle(InPacket&) const
	{
		Stage::get().get_player().recalc_stats(false);
	}

	void UpdateSkillHandler::handle(InPacket& recv) const
	{
		recv.skip(3);

		int32_t skillid = recv.read_int();
		int32_t level = recv.read_int();
		int32_t masterlevel = recv.read_int();
		int64_t expire = recv.read_long();

		Stage::get().get_player().change_skill(skillid, level, masterlevel, expire);

		if (auto skillbook = UI::get().get_element<UISkillBook>())
			skillbook->update_skills(skillid);

		UI::get().enable();
	}

	void SkillMacrosHandler::handle(InPacket& recv) const
	{
		uint8_t size = recv.read_byte();

		for (uint8_t i = 0; i < size; i++)
		{
			recv.read_string();	// name
			recv.read_byte();	// 'shout' byte
			recv.read_int();	// skill 1
			recv.read_int();	// skill 2
			recv.read_int();	// skill 3
		}
	}

	void AddCooldownHandler::handle(InPacket& recv) const
	{
		int32_t skill_id = recv.read_int();
		int16_t cooltime = recv.read_short();

		Stage::get().get_player().add_cooldown(skill_id, cooltime);
	}

	void KeymapHandler::handle(InPacket& recv) const
	{
		recv.skip(1);

		for (uint8_t i = 0; i < 90; i++)
		{
			uint8_t type = recv.read_byte();
			int32_t action = recv.read_int();

			UI::get().add_keymapping(i, type, action);
		}
	}
}