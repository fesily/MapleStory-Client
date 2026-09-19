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
#include "CashShopHandlers.h"

#include "Helpers/CashShopParser.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/Window.h"

#include "../../MapleStory.h"

#include <iostream>

namespace ms
{
	namespace
	{
		// Commodity flag bits, written as a mask by writeModifiedCashItem
		// (CommodityFlag.java:22-56)
		enum Commodity : int32_t
		{
			ITEM_ID = 1 << 0,
			COUNT = 1 << 1,
			PRICE = 1 << 2,
			BONUS = 1 << 3,
			PRIORITY = 1 << 4,
			PERIOD = 1 << 5,
			MAPLE_POINT = 1 << 6,
			MESO = 1 << 7,
			FOR_PREMIUM_USER = 1 << 8,
			COMMODITY_GENDER = 1 << 9,
			ON_SALE = 1 << 10,
			CLASS = 1 << 11,
			LIMIT = 1 << 12,
			PB_CASH = 1 << 13,
			PB_POINT = 1 << 14,
			PB_GIFT = 1 << 15,
			PACKAGE_SN = 1 << 16
		};

		// Modified cash shop items are flag driven: every record starts with its sn and
		// the flag mask, which tells which of the fields below are present. The fields
		// are written in flag sort order, so the order here is the order on the wire
		// (PacketCreator.writeModifiedCashItem, PacketCreator.java:7229-7266)
		void parse_modified_cash_item(InPacket& recv)
		{
			recv.skip_int();	// sn

			int32_t flags = recv.read_int();

			if (flags & ~0x1FFFF)
				LOG(LOG_NETWORK, "[SetCashShopHandler] Unknown commodity flags: " << flags);

			if (flags & ITEM_ID)
				recv.skip_int();	// itemid
			if (flags & COUNT)
				recv.skip_short();	// count
			if (flags & PRIORITY)
				recv.skip_byte();	// priority
			if (flags & PRICE)
				recv.skip_int();	// price
			if (flags & BONUS)
				recv.skip_byte();	// bonus
			if (flags & PERIOD)
				recv.skip_short();	// period
			if (flags & MAPLE_POINT)
				recv.skip_int();	// maple point
			if (flags & MESO)
				recv.skip_int();	// meso
			if (flags & FOR_PREMIUM_USER)
				recv.skip_byte();	// premium user only
			if (flags & COMMODITY_GENDER)
				recv.skip_byte();	// gender
			if (flags & ON_SALE)
				recv.skip_byte();	// on sale
			if (flags & CLASS)
				recv.skip_byte();	// tab
			if (flags & LIMIT)
				recv.skip_byte();	// limited sale
			if (flags & PB_CASH)
				recv.skip_short();	// pb cash
			if (flags & PB_POINT)
				recv.skip_short();	// pb point
			if (flags & PB_GIFT)
				recv.skip_short();	// pb gift
			if (flags & PACKAGE_SN)
			{
				// A package writes the amount of items it contains, then their sns
				int8_t packagesize = recv.read_byte();

				for (int8_t i = 0; i < packagesize; i++)
					recv.skip_int();	// package item sn
			}
		}
	}

	void SetCashShopHandler::handle(InPacket& recv) const
	{
		CashShopParser::parseCharacterInfo(recv);

		recv.skip_byte();	// Not MTS
		recv.skip_string();	// account_name
		recv.skip_int();

		int16_t specialcashitem_size = recv.read_short();

		LOG(LOG_NETWORK, "[SetCashShopHandler] Modified cash items: " << specialcashitem_size);

		for (size_t i = 0; i < specialcashitem_size; i++)
			parse_modified_cash_item(recv);

		recv.skip(121);	// fixed handshake blob (PacketCreator.java:7201)

		// Each of the eight tabs carries a most seller list of exactly five entries for
		// both genders; the packet does not send a count for them
		// (World.getMostSellerCashItems, World.java:1409-1440)
		for (size_t cat = 1; cat <= 8; cat++)
		{
			for (size_t gender = 0; gender < 2; gender++)
			{
				for (size_t in = 0; in < 5; in++)
				{
					recv.skip_int(); // category
					recv.skip_int(); // gender
					recv.skip_int(); // commoditysn
				}
			}
		}

		recv.skip_int();
		recv.skip_short();
		recv.skip_byte();
		recv.skip_int();

		transition();

		UI::get().change_state(UI::State::CASHSHOP);
	}

	void SetCashShopHandler::transition() const
	{
		Constants::Constants::get().set_viewwidth(1024);
		Constants::Constants::get().set_viewheight(768);

		float fadestep = 0.025f;

		Window::get().fadeout(
			fadestep,
			[]()
			{
				GraphicsGL::get().clear();

				Stage::get().load(-1, 0);

				UI::get().enable();
				Timer::get().start();
				GraphicsGL::get().unlock();
			}
		);

		GraphicsGL::get().lock();
		Stage::get().clear();
		Timer::get().start();
	}
}