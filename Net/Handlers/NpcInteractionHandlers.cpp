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
#include "NpcInteractionHandlers.h"

#include "../../IO/UI.h"

#include "../../IO/UITypes/UINotice.h"
#include "../../IO/UITypes/UINpcTalk.h"
#include "../../IO/UITypes/UIShop.h"

#include "../../MapleStory.h"

#include <iostream>

namespace ms
{
	namespace
	{
		// Reads the payload of an OnAskQuiz packet; a failed quiz carries only its
		// answer code (PacketCreator.java:3421-3437)
		std::string parse_quiz(InPacket& recv)
		{
			int8_t rescode = recv.read_byte();

			if (rescode != 0)
			{
				LOG(LOG_NETWORK, "[NpcDialogueHandler] Quiz failed with answer code {}", (int)rescode);
				return "";
			}

			std::string title = recv.read_string();
			std::string problem = recv.read_string();
			std::string hint = recv.read_string();
			int16_t mininput = recv.read_short();
			int16_t maxinput = recv.read_short();
			int32_t remaining = recv.read_int();

			LOG(LOG_NETWORK, "[NpcDialogueHandler] Quiz '{}' is not wired up: answer between {} and {}, {} ms to answer",
				title, mininput, maxinput, remaining);

			return problem;
		}
	}

	void NpcDialogueHandler::handle(InPacket& recv) const
	{
		recv.skip(1);	// speaker type

		NpcTalkDialogue dialogue;

		dialogue.npcid = recv.read_int();
		dialogue.msgtype = recv.read_byte();
		dialogue.speaker = recv.read_byte();

		LOG(LOG_NETWORK, "[NpcDialogueHandler] npc=[{}] msgType=[{}] speaker=[{}]",
			dialogue.npcid, static_cast<int32_t>(dialogue.msgtype), static_cast<int32_t>(dialogue.speaker));

		if (dialogue.msgtype == 0x06)
		{
			// A quiz has no talk text, it sends its question in the payload
			dialogue.text = parse_quiz(recv);
		}
		else
		{
			// The dimensional mirror puts a hardcoded int in front of its text
			// (PacketCreator.getDimensionalMirror, PacketCreator.java:3344-3352)
			if (dialogue.msgtype == 0x0E)
				recv.skip_int();

			dialogue.text = recv.read_string();
		}

		// Read the rest of the payload, which depends on the msgType
		// (PacketCreator.getNPCTalk and the getNPCTalk* methods, PacketCreator.java:3333-3419)
		switch (dialogue.msgtype)
		{
			case 0x00:
			{
				// The two trailing bytes are the prev and next button flags
				// (NPCConversationManager.java:156-171)
				if (recv.length() >= 2)
				{
					dialogue.prev = recv.read_byte();
					dialogue.next = recv.read_byte();
				}
				else
				{
					LOG(LOG_NETWORK, "[NpcDialogueHandler] msgType 0 dialog without button flags");
				}

				break;
			}
			case 0x02:
			{
				// getNPCTalkText sends the text the input box starts with, followed by
				// an int (PacketCreator.java:3383-3392)
				dialogue.textdefault = recv.read_string();
				recv.skip_int();
				break;
			}
			case 0x03:
			{
				// getNPCTalkNum sends the default, minimum and maximum of the number
				// input, followed by an int (PacketCreator.java:3369-3381)
				dialogue.numdefault = recv.read_int();
				dialogue.nummin = recv.read_int();
				dialogue.nummax = recv.read_int();
				recv.skip_int();
				break;
			}
			case 0x07:
			{
				// msgType 7 is either the cosmetic style list or the speed quiz; only one
				// of the two payloads matches what is left of the packet
				// (PacketCreator.java:3355-3366 and :3439-3454)
				int8_t count = recv.read_byte();

				if (count > 0 && recv.length() == static_cast<size_t>(4 * count))
				{
					for (int8_t i = 0; i < count; i++)
						recv.skip_int();	// style

					LOG(LOG_NETWORK, "[NpcDialogueHandler] Cosmetic style dialog with {} styles is not wired up",
						(int)count);
				}
				else
				{
					// The first byte was the answer code of a speed quiz
					if (count == 0 && recv.length() >= 20)
						recv.skip(20);	// type, answer, correct, remaining and time to answer

					LOG(LOG_NETWORK, "[NpcDialogueHandler] Speed quiz is not wired up, answer code {}", (int)count);
				}

				break;
			}
			default:
			{
				break;
			}
		}

		if (recv.length() > 0)
			LOG(LOG_NETWORK, "[NpcDialogueHandler] msgType {}: {} unconsumed bytes",
				(int)dialogue.msgtype, recv.length());

		UI::get().emplace<UINpcTalk>();
		UI::get().enable();

		if (auto npctalk = UI::get().get_element<UINpcTalk>())
			npctalk->change_text(dialogue);
	}

	void OpenNpcShopHandler::handle(InPacket& recv) const
	{
		int32_t npcid = recv.read_int();
		auto oshop = UI::get().get_element<UIShop>();

		if (!oshop)
			return;

		UIShop& shop = *oshop;

		shop.reset(npcid);

		int16_t size = recv.read_short();

		for (int16_t i = 0; i < size; i++)
		{
			int32_t itemid = recv.read_int();
			int32_t price = recv.read_int();
			int32_t pitch = recv.read_int();
			int32_t time = recv.read_int();

			recv.skip(4);

			bool norecharge = recv.read_short() == 1;

			if (norecharge)
			{
				int16_t buyable = recv.read_short();

				shop.add_item(itemid, price, pitch, time, buyable);
			}
			else
			{
				recv.skip(4);

				int16_t rechargeprice = recv.read_short();
				int16_t slotmax = recv.read_short();

				shop.add_rechargable(itemid, price, pitch, time, rechargeprice, slotmax);
			}
		}
	}

	void ConfirmShopTransactionHandler::handle(InPacket& recv) const
	{
		int8_t code = recv.read_byte();

		// A shop transaction answers with a single result code
		// (PacketCreator.shopTransaction, PacketCreator.java:2420-2435); the codes are
		// sent from the buy, sell and recharge methods of net/server/Shop.java
		switch (code)
		{
			case 0x00:
			case 0x08:
			{
				// The transaction went through; with a bought, sold or recharged item
				// the server also sends the inventory change
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] Transaction done, result code {}", (int)code);
				break;
			}
			case 0x01:
			case 0x05:
			{
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] The shop does not have enough in stock");
				UI::get().emplace<UIOk>("You don't have enough in stock.", [](bool) {});
				break;
			}
			case 0x02:
			{
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] Not enough mesos");
				UI::get().emplace<UIOk>("You do not have enough mesos.", [](bool) {});
				break;
			}
			case 0x03:
			{
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] The inventory is full");
				UI::get().emplace<UIOk>("Please check if your inventory is full or not.", [](bool) {});
				break;
			}
			case 0x06:
			case 0x07:
			{
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] Trade error, result code {}", (int)code);
				UI::get().emplace<UIOk>("Due to an error, the trade did not happen.", [](bool) {});
				break;
			}
			case 0x0D:
			{
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] Not enough items");
				UI::get().emplace<UIOk>("You need more items.", [](bool) {});
				break;
			}
			default:
			{
				LOG(LOG_NETWORK, "[ConfirmShopTransactionHandler] Unknown result code {}", (int)code);
				break;
			}
		}
	}
}
