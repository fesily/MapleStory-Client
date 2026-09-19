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
#include "MessagingHandlers.h"

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"

#include "../../IO/UITypes/UIChatBar.h"
#include "../../IO/UITypes/UIStatusMessenger.h"

#include "../../MapleStory.h"

#include <iostream>

namespace
{
	// Consume an item block as written by PacketCreator.addItemInfo(p, item, true)
	// (PacketCreator.java:390-482) and return the id of the item it describes.
	// The block is parsed branch by branch so that no byte of it stays in the packet.
	int32_t skip_item_info(ms::InPacket& recv)
	{
		int8_t type = recv.read_byte();	// 1 = equip, 3 = pet, 2 = everything else (Item.java:116)
		int32_t itemid = recv.read_int();
		bool cash = recv.read_bool();

		if (cash)
			recv.skip_long();	// cash id

		recv.skip_long();	// expiration time

		if (type == 3)	// pet
		{
			recv.skip_padded_string(13);	// name
			recv.skip_byte();				// level
			recv.skip_short();				// closeness
			recv.skip_byte();				// fullness
			recv.skip_long();				// expiration time
			recv.skip_short();				// pet attribute
			recv.skip_short();				// pet skill
			recv.skip_int();				// remaining life
			recv.skip_short();				// unused

			return itemid;
		}

		if (type == 1)	// equip
		{
			recv.skip(2);		// upgrade slots, level
			recv.skip(30);		// fifteen equipment stats, one short each
			recv.skip_string();	// owner
			recv.skip_short();	// flags

			if (cash)
				recv.skip(10);
			else
				recv.skip(18);	// unused, item level, item exp, vicious, unused

			recv.skip_long();	// expiration time
			recv.skip_int();	// unused

			return itemid;
		}

		recv.skip_short();	// quantity
		recv.skip_string();	// owner
		recv.skip_short();	// flags

		// Throwing stars and bullets carry four more bytes
		if (itemid / 10000 == 207 || itemid / 10000 == 233)
			recv.skip(8);

		return itemid;
	}
}

namespace ms
{
	// Modes, with the bytes the server writes after the mode byte:
	// 0  - item gain (PacketCreator.java:1760-1764), meso gain (:1721-1727),
	//      inventory full (:3559-3560), item unavailable (:3561-3568)
	// 1  - quest forfeited (:2861-2863), completed (:2873-2876), updated (:2920-2933)
	// 2  - expired cash item (:6738-6739)
	// 3  - experience gain (:1668-1685)
	// 4  - fame gain (:1696-1697)
	// 5  - meso gain in chat (:1724-1727)
	// 6  - guild points (:6026-6027)
	// 7  - item use message (:6033-6034)
	// 9  - info text (:6283-6284, :6604-6605, :6828-6829)
	// 10 - area info (:6018-6020), dojo info (:6596-6599, :6649-6651)
	void ShowStatusInfoHandler::handle(InPacket& recv) const
	{
		int8_t mode = recv.read_byte();

		if (mode == 0)
		{
			int8_t mode2 = recv.read_byte();

			if (mode2 == -1)	// 0xFF, getShowInventoryFull (:3559-3560)
			{
				recv.skip(8);	// two unused ints

				show_status(Color::Name::WHITE, "You can't get anymore items.");
			}
			else if (mode2 == -2)	// 0xFE, showItemUnavailable (:3561-3568)
			{
				recv.skip(8);	// two unused ints

				show_status(Color::Name::WHITE, "You can't pick up this item.");
			}
			else if (mode2 == 0)	// getShowItemGain without in chat (:1759-1764)
			{
				int32_t itemid = recv.read_int();
				int32_t qty = recv.read_int();

				recv.skip(8);	// two unused ints

				const ItemData& idata = ItemData::get(itemid);

				if (!idata.is_valid())
					return;

				std::string name = idata.get_name();

				if (name.length() > 21)
				{
					name.substr(0, 21);
					name += "..";
				}

				InventoryType::Id type = InventoryType::by_item_id(itemid);

				std::string tab = "";

				switch (type)
				{
					case InventoryType::Id::EQUIP:
						tab = "Eqp";
						break;
					case InventoryType::Id::USE:
						tab = "Use";
						break;
					case InventoryType::Id::SETUP:
						tab = "Setup";
						break;
					case InventoryType::Id::ETC:
						tab = "Etc";
						break;
					case InventoryType::Id::CASH:
						tab = "Cash";
						break;
					default:
						tab = "UNKNOWN";
						break;
				}

				// TODO: show_status(Color::Name::WHITE, "You have lost items in the " + tab + " tab (" + name + " " + std::to_string(qty) + ")");

				if (qty < 0)
					show_status(Color::Name::WHITE, "You have lost an item in the " + tab + " tab (" + name + ")");
				else if (qty == 1)
					show_status(Color::Name::WHITE, "You have gained an item in the " + tab + " tab (" + name + ")");
				else
					show_status(Color::Name::WHITE, "You have gained items in the " + tab + " tab (" + name + " " + std::to_string(qty) + ")");
			}
			else if (mode2 == 1)	// getShowMesoGain without in chat (:1720-1727)
			{
				recv.skip(1);	// high byte of the short the server writes for mesos

				int32_t gain = recv.read_int();

				recv.skip_short();	// unused

				std::string sign = (gain < 0) ? "-" : "+";

				show_status(Color::Name::WHITE, "You have gained mesos (" + sign + std::to_string(gain) + ")");
			}
			else
			{
				LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: unknown mode 0 sub mode [" << static_cast<int32_t>(mode2) << "], " << recv.length() << " bytes dropped.");

				recv.skip(recv.length());
			}
		}
		else if (mode == 1)	// quest status
		{
			int16_t quest = recv.read_short();
			int8_t status = recv.read_byte();

			// completeQuest (:2873-2876) writes the completion time and nothing else after the
			// status byte, updateQuest (:2920-2933) writes the progress string and five bytes.
			if (status == 2 && recv.length() == 8)
			{
				int64_t time = recv.read_long();

				LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: quest [" << quest << "] completed at [" << time << "].");
			}
			else if (recv.available())
			{
				std::string progress = recv.read_string();

				recv.skip(5);	// unused

				LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: quest [" << quest << "] status [" << static_cast<int32_t>(status) << "], progress [" << progress << "].");
			}
			else
			{
				LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: quest [" << quest << "] status [" << static_cast<int32_t>(status) << "].");
			}

			std::string message = "Quest " + std::to_string(quest);

			if (status == 2)
				message += " completed.";
			else if (status == 1)
				message += " started.";
			else
				message += " forfeited.";

			show_status(Color::Name::WHITE, message);
		}
		else if (mode == 2)	// itemExpired (:6738-6739)
		{
			int32_t itemid = recv.read_int();

			const ItemData& idata = ItemData::get(itemid);
			std::string name = idata.is_valid() ? idata.get_name() : std::to_string(itemid);

			show_status(Color::Name::WHITE, "Cash item " + name + " has expired.");
		}
		else if (mode == 3)	// getShowExpGain (:1668-1685)
		{
			bool white = recv.read_bool();
			int32_t gain = recv.read_int();
			bool inchat = recv.read_bool();
			int32_t bonus = recv.read_int();		// bonus event exp
			int8_t kill = recv.read_byte();			// third monster kill event
			int8_t unused = recv.read_byte();		// unused
			int32_t wedding = recv.read_int();		// wedding bonus
			int8_t questrate = 0;					// quest bonus rate, only written in chat

			if (inchat)
				questrate = recv.read_byte();

			int8_t partytype = recv.read_byte();	// 0 = party bonus, 100 = 1x, 200 = 2x bonus exp
			int32_t party = recv.read_int();		// party bonus
			int32_t equip = recv.read_int();		// equip bonus
			int32_t cafe = recv.read_int();			// Internet cafe bonus
			int32_t rainbow = recv.read_int();		// Rainbow week bonus

			LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: experience gain [" << gain << "], in chat [" << inchat
				<< "], bonus [" << bonus << "], kill [" << static_cast<int32_t>(kill)
				<< "], unused [" << static_cast<int32_t>(unused) << "], wedding [" << wedding
				<< "], quest rate [" << static_cast<int32_t>(questrate) << "], party type [" << static_cast<int32_t>(partytype)
				<< "], party [" << party << "], equip [" << equip << "], cafe [" << cafe
				<< "], rainbow [" << rainbow << "].");

			show_status(white ? Color::Name::WHITE : Color::Name::YELLOW, "You have gained experience (+" + std::to_string(gain) + ")");

			if (bonus > 0)
				show_status(Color::Name::YELLOW, "+ Bonus EXP (+" + std::to_string(bonus) + ")");

			if (party > 0)
				show_status(Color::Name::YELLOW, "+ Party Bonus EXP (+" + std::to_string(party) + ")");

			if (equip > 0)
				show_status(Color::Name::YELLOW, "+ Equipment Bonus EXP (+" + std::to_string(equip) + ")");
		}
		else if (mode == 4)	// getShowFameGain (:1696-1697)
		{
			int32_t gain = recv.read_int();
			std::string sign = (gain < 0) ? "-" : "+";

			// TODO: Lose fame?
			show_status(Color::Name::WHITE, "You have gained fame. (" + sign + std::to_string(gain) + ")");
		}
		else if (mode == 5)	// getShowMesoGain in chat (:1724-1727)
		{
			int32_t gain = recv.read_int();

			recv.skip_short();	// unused

			std::string sign = (gain < 0) ? "-" : "+";

			show_status(Color::Name::WHITE, "You have gained mesos (" + sign + std::to_string(gain) + ")");
		}
		else if (mode == 6)	// getGPMessage (:6026-6027)
		{
			int32_t gain = recv.read_int();
			std::string sign = (gain < 0) ? "-" : "+";

			show_status(Color::Name::WHITE, "You have gained guild points (" + sign + std::to_string(gain) + ")");
		}
		else if (mode == 7)	// getItemMessage (:6033-6034)
		{
			int32_t itemid = recv.read_int();

			const ItemData& idata = ItemData::get(itemid);

			if (idata.is_valid())
				show_status(Color::Name::WHITE, idata.get_name() + ": " + idata.get_desc());
			else
				LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: item message for unknown item [" << itemid << "].");
		}
		else if (mode == 9)	// info text
		{
			// showInfoText (:6283-6284) and getDojoInfoMessage (:6604-6605) write a length
			// prefixed string, bunnyPacket (:6828-6829) writes thirteen bytes without a length.
			int16_t declared = recv.inspect_short();
			size_t remaining = recv.length();

			std::string text;

			if (declared >= 0 && static_cast<size_t>(declared) == remaining - 2)
				text = recv.read_string();
			else
				text = recv.read_padded_string(13);

			LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: info text [" << text << "].");

			show_status(Color::Name::WHITE, text);
		}
		else if (mode == 10)	// updateAreaInfo (:6018-6020) and dojo info (:6596-6599, :6649-6651)
		{
			int16_t info = recv.read_short();
			std::string text = recv.read_string();

			LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: info number [" << info << "], text [" << text << "].");

			show_status(Color::Name::WHITE, text);
		}
		else
		{
			LOG(LOG_NETWORK, "[ShowStatusInfoHandler]: unknown mode [" << static_cast<int32_t>(mode) << "], " << recv.length() << " bytes dropped.");

			recv.skip(recv.length());
		}
	}

	void ShowStatusInfoHandler::show_status(Color::Name color, const std::string& message) const
	{
		if (auto messenger = UI::get().get_element<UIStatusMessenger>())
			messenger->show_status(color, message);
	}

	// Types, with the bytes the server writes after the type byte:
	// 0, 1, 2, 5 - notice, popup, megaphone, pink text (:1264-1279)
	// 3          - super megaphone (:1270-1272)
	// 4          - scrolling ticker (:1266-1269)
	// 6          - lightblue text (:1273-1274)
	// 7          - broadcasting NPC (:1275-1276)
	// 8          - item megaphone (:6131-6140)
	// 0x0A       - multi megaphone (:6370-6388)
	// 0x0B       - gachapon (:1324-1329)
	void ServerMessageHandler::handle(InPacket& recv) const
	{
		int8_t type = recv.read_byte();

		// Only the scrolling ticker is written with the extra flag byte
		// (PacketCreator.java:1264-1269 with servermessage = true, used by
		// serverMessage(String) at :1210-1212); no serverNotice overload sets it.
		if (type == 4)
		{
			int8_t ticker = recv.read_byte();

			if (ticker != 1)
				LOG(LOG_NETWORK, "[ServerMessageHandler]: unexpected ticker flag [" << static_cast<int32_t>(ticker) << "].");
		}

		std::string message = recv.read_string();

		if (type == 3)	// super megaphone
		{
			int8_t channel = recv.read_byte();
			bool megaEar = recv.read_bool();

			std::string text = "[Super Megaphone] " + message;

			LOG(LOG_NETWORK, "[ServerMessageHandler]: super megaphone on channel [" << static_cast<int32_t>(channel) + 1 << "], ear [" << megaEar << "]: " << text);

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(text.c_str(), UIChatBar::MessageType::YELLOW);
		}
		else if (type == 4)	// scrolling ticker
		{
			UI::get().set_scrollnotice(message);
		}
		else if (type == 5)	// pink text
		{
			// TODO: Is this actually white?
			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(message.c_str(), UIChatBar::MessageType::WHITE);
		}
		else if (type == 6)	// lightblue text
		{
			int32_t unused = recv.read_int();

			LOG(LOG_NETWORK, "[ServerMessageHandler]: lightblue text [" << message << "], unused [" << unused << "].");

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(message.c_str(), UIChatBar::MessageType::WHITE);
		}
		else if (type == 7)	// broadcasting NPC
		{
			int32_t npc = recv.read_int();

			LOG(LOG_NETWORK, "[ServerMessageHandler]: message from NPC [" << npc << "]: " << message);

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(message.c_str(), UIChatBar::MessageType::WHITE);
		}
		else if (type == 8)	// item megaphone
		{
			int8_t channel = recv.read_byte();
			bool whisper = recv.read_bool();
			int8_t position = recv.read_byte();

			LOG(LOG_NETWORK, "[ServerMessageHandler]: item megaphone on channel [" << static_cast<int32_t>(channel) + 1
				<< "], whisper [" << whisper << "], slot [" << static_cast<int32_t>(position) << "]: " << message);

			if (recv.available())	// the server only writes the item block when an item is attached
			{
				int32_t itemid = skip_item_info(recv);

				LOG(LOG_NETWORK, "[ServerMessageHandler]: item megaphone attachment [" << itemid << "].");
			}

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(message.c_str(), UIChatBar::MessageType::YELLOW);
		}
		else if (type == 0x0A)	// multi megaphone
		{
			int8_t lines = recv.read_byte();

			std::string text = message;

			for (int8_t i = 1; i < lines; i++)
				text += " " + recv.read_string();

			recv.skip(10);		// channel of every line
			bool showEar = recv.read_bool();
			recv.read_byte();	// unused

			LOG(LOG_NETWORK, "[ServerMessageHandler]: multi megaphone, ear [" << showEar << "]: " << text);

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(text.c_str(), UIChatBar::MessageType::YELLOW);
		}
		else if (type == 0x0B)	// gachapon
		{
			int32_t unused = recv.read_int();
			std::string town = recv.read_string();
			int32_t itemid = skip_item_info(recv);

			const ItemData& idata = ItemData::get(itemid);
			std::string itemname = idata.is_valid() ? idata.get_name() : std::to_string(itemid);

			std::string text = message + " " + itemname + " (" + town + ")";

			LOG(LOG_NETWORK, "[ServerMessageHandler]: gachapon [" << text << "], unused [" << unused << "].");

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(text.c_str(), UIChatBar::MessageType::YELLOW);
		}
		else	// notice, popup and megaphone text
		{
			LOG(LOG_NETWORK, "[ServerMessageHandler]: notice [" << static_cast<int32_t>(type) << "]: " << message);

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(message.c_str(), UIChatBar::MessageType::YELLOW);

			if (recv.available())
			{
				LOG(LOG_NETWORK, "[ServerMessageHandler]: unknown type [" << static_cast<int32_t>(type) << "], " << recv.length() << " bytes dropped.");

				recv.skip(recv.length());
			}
		}
	}

	void WeekEventMessageHandler::handle(InPacket& recv) const
	{
		recv.read_byte(); // TODO: Always 0xFF, Check this!

		std::string message = recv.read_string();

		recv.skip_short();	// unused (PacketCreator.java:5412)

		static const std::string MAPLETIP = "[MapleTip]";

		if (message.substr(0, MAPLETIP.length()).compare("[MapleTip]"))
			message = "[Notice] " + message;

		if (auto chatbar = UI::get().get_element<UIChatBar>())
			chatbar->show_message(message.c_str(), UIChatBar::MessageType::YELLOW);
	}

	void ChatReceivedHandler::handle(InPacket& recv) const
	{
		int32_t charid = recv.read_int();
		bool gm = recv.read_bool();	// white chat of a GM (Character.java:9639-9641)
		std::string message = recv.read_string();
		int8_t type = recv.read_byte();	// 'show' byte the sender used (GeneralChatHandler.java:61)

		if (auto character = Stage::get().get_character(charid))
		{
			message = character->get_name() + " : " + message;
			character->speak(message);
		}

		UIChatBar::MessageType linetype = UIChatBar::MessageType::WHITE;

		if (!gm)
		{
			if (type >= static_cast<int8_t>(UIChatBar::MessageType::WHITE) && type <= static_cast<int8_t>(UIChatBar::MessageType::YELLOW))
			{
				linetype = static_cast<UIChatBar::MessageType>(type);
			}
			else if (type != 0)	// the plain line type is sent as zero
			{
				LOG(LOG_NETWORK, "[ChatReceivedHandler]: unknown line type [" << static_cast<int32_t>(type) << "].");

				linetype = UIChatBar::MessageType::RED;
			}
		}

		if (auto chatbar = UI::get().get_element<UIChatBar>())
			chatbar->show_message(message.c_str(), linetype);
	}

	void ScrollResultHandler::handle(InPacket& recv) const
	{
		int32_t cid = recv.read_int();
		bool success = recv.read_bool();
		bool destroyed = recv.read_bool();
		bool legendary = recv.read_bool();	// legendary spirit was used
		bool white = recv.read_bool();		// white scroll was used

		LOG(LOG_NETWORK, "[ScrollResultHandler]: scroll for [" << cid << "], success [" << success
			<< "], destroyed [" << destroyed << "], legendary spirit [" << legendary
			<< "], white scroll [" << white << "].");

		CharEffect::Id effect;
		Messages::Type message;

		if (success)
		{
			effect = CharEffect::Id::SCROLL_SUCCESS;
			message = Messages::Type::SCROLL_SUCCESS;
		}
		else
		{
			effect = CharEffect::Id::SCROLL_FAILURE;

			if (destroyed)
				message = Messages::Type::SCROLL_DESTROYED;
			else
				message = Messages::Type::SCROLL_FAILURE;
		}

		Stage::get().show_character_effect(cid, effect);

		if (Stage::get().is_player(cid))
		{
			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->show_message(Messages::messages[message], UIChatBar::MessageType::RED);

			UI::get().enable();
		}
	}

	// Modes, with the bytes the server writes after the mode byte:
	// 1    - buff effect (:3481-3485) or berserk (:3490-3495)
	// 3    - item gain in chat (:1752-1756)
	// 4    - pet level up (:4505-4510)
	// 0x0A - recovery (:6250-6254)
	// 0x0D - monster card gain (:6046-6050)
	// 0x12 - intro (:6076-6080)
	// 0x15 - wheels of fortune left (:6265-6269)
	// 0x17 - info (:6083-6088)
	// 16   - maker skill (:6224-6229)
	// rest - payload free effects written by showSpecialEffect (:6218-6221)
	void ShowItemGainInChatHandler::handle(InPacket& recv) const
	{
		int8_t mode1 = recv.read_byte();

		if (mode1 == 1 || mode1 == 2 || mode1 == 5)	// buff effect / berserk
		{
			int32_t skillid = recv.read_int();

			recv.skip_byte();	// 0xA9, the buff effect marker

			// showOwnBuffEffect ends with one more byte, showOwnBerserk appends the skill
			// level and the berserk flag instead.
			recv.skip(recv.length());

			Stage::get().get_combat().show_player_buff(skillid);
		}
		else if (mode1 == 3)	// item gain in chat
		{
			int8_t mode2 = recv.read_byte();

			if (mode2 == 1)
			{
				int32_t itemid = recv.read_int();
				int32_t qty = recv.read_int();

				const ItemData& idata = ItemData::get(itemid);

				if (!idata.is_valid())
					return;

				std::string name = idata.get_name();
				std::string sign = (qty < 0) ? "-" : "+";
				std::string message = "Gained an item: " + name + " (" + sign + std::to_string(qty) + ")";

				if (auto chatbar = UI::get().get_element<UIChatBar>())
					chatbar->show_message(message.c_str(), UIChatBar::MessageType::BLUE);
			}
			else
			{
				LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: unknown item gain mode [" << static_cast<int32_t>(mode2) << "], " << recv.length() << " bytes dropped.");

				recv.skip(recv.length());
			}
		}
		else if (mode1 == 4)	// pet level up
		{
			int8_t unused = recv.read_byte();
			int8_t index = recv.read_byte();

			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: pet [" << static_cast<int32_t>(index) << "] leveled up, unused [" << static_cast<int32_t>(unused) << "].");
		}
		else if (mode1 == 0x0A)	// recovery
		{
			int8_t heal = recv.read_byte();

			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: recovered [" << static_cast<int32_t>(heal) << "] hp.");
		}
		else if (mode1 == 0x0D)	// monster card gain
		{
			Stage::get().get_player().show_effect_id(CharEffect::Id::MONSTER_CARD);
		}
		else if (mode1 == 0x12)	// intro
		{
			std::string path = recv.read_string();

			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: intro effect [" << path << "] is not displayed.");
		}
		else if (mode1 == 0x15)	// wheels of fortune left
		{
			int8_t left = recv.read_byte();

			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: [" << static_cast<int32_t>(left) << "] wheels of fortune left.");
		}
		else if (mode1 == 0x17)	// info
		{
			std::string path = recv.read_string();
			int32_t unused = recv.read_int();

			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: info effect [" << path << "] with [" << unused << "] is not displayed.");
		}
		else if (mode1 == 16)	// maker skill
		{
			int32_t failed = recv.read_int();

			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: maker skill effect, failed [" << failed << "] is not displayed.");
		}
		else	// payload free effects, showSpecialEffect only writes the effect byte
		{
			LOG(LOG_NETWORK, "[ShowItemGainInChatHandler]: effect [" << static_cast<int32_t>(mode1) << "] is not displayed.");
		}
	}
}