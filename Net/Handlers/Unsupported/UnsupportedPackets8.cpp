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
#include "../../InPacket.h"
#include "../../ServerOpcodes.h"
#include "../../UnsupportedPackets.h"

namespace ms
{
	namespace Unsupported
	{
		// Opcodes handled by this table (server is authoritative for every layout):
		// 314 PLAYER_INTERACTION, 315 TOURNAMENT, 316 TOURNAMENT_MATCH_TABLE, 317 TOURNAMENT_SET_PRIZE, 318 TOURNAMENT_UEW, 322 PARCEL, 324 QUERY_CASH_RESULT, 325 CASHSHOP_OPERATION, 328 CASHSHOP_CHECK_NAME_CHANGE, 329 CASHSHOP_CHECK_NAME_CHANGE_POSSIBLE_RESULT, 331 CASHSHOP_CHECK_TRANSFER_WORLD_POSSIBLE_RESULT, 333 CASHSHOP_CASH_ITEM_GACHAPON_RESULT, 336 AUTO_HP_POT, 337 AUTO_MP_POT, 341 SEND_TV, 342 REMOVE_TV, 343 ENABLE_TV, 347 MTS_OPERATION2, 348 MTS_OPERATION, 349 MAPLELIFE_RESULT, 350 MAPLELIFE_ERROR, 354 VICIOUS_HAMMER, 358 VEGA_SCROLL, 4096 UPDATE_HPMPAALERT
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		//
		// Most of these opcodes start with a sub-type byte and continue with a shape
		// which depends on it, so those parsers dispatch on the sub-type exactly like
		// their encoders do. The two remaining shapes which the payload itself cannot
		// tell apart (the two UpdateMerchant encoders and the owner/visitor forms of
		// the hired merchant room) are separated by checking which one consumes the
		// payload exactly, see adopt_shape below.
		//
		// 4096 UPDATE_HPMPAALERT is registered here even though the client's dispatch
		// bound (PacketSwitch::NUM_HANDLERS = 500) never routes it: the parser keeps
		// the layout documented, see register_packets8.
		namespace
		{
			// ---------------------------------------------------------------
			// Shared sub-structures
			// ---------------------------------------------------------------

			// One character look as written by PacketCreator.addCharLook
			// (PacketCreator.java:215) with mega = false, plus the equipment
			// block of addCharEquips (PacketCreator.java:288): two slot lists
			// which both end with a 0xFF byte, the covered weapon and three pets.
			void consume_char_equips(InPacket& recv)
			{
				// Equipped items, ended by a 0xFF slot byte
				while (recv.read_byte() != -1)
					recv.read_int(); // item id

				// Items covered by a cash item, same shape
				while (recv.read_byte() != -1)
					recv.read_int(); // item id

				recv.read_int(); // covered weapon id, 0 when there is none
				recv.read_int(); // pet 1 item id
				recv.read_int(); // pet 2 item id
				recv.read_int(); // pet 3 item id
			}

			void consume_char_look(InPacket& recv)
			{
				recv.read_byte(); // gender
				recv.read_byte(); // skin color
				recv.read_int(); // face
				recv.read_bool(); // mega flag, always 1 for every caller here
				recv.read_int(); // hair
				consume_char_equips(recv);
			}

			// One item description as written by PacketCreator.addItemInfo
			// (PacketCreator.java:390) with zeroPosition = true: no slot field,
			// then the item type, the id, the cash flag, an optional unique id,
			// the expiration and the pet, equip or stackable detail of the item.
			// The item type comes from Item.getItemType (Item.java:116): 1 when
			// the item is an equip, 3 when it has a pet id, 2 otherwise.
			void consume_item_info(InPacket& recv)
			{
				int8_t type = recv.read_byte();
				int32_t item_id = recv.read_int();
				bool cash = recv.read_bool();

				if (cash)
					recv.read_long(); // pet id, ring id or cash id

				recv.read_long(); // expiration time

				if (type == 3) // pet
				{
					recv.read_padded_string(13); // name
					recv.read_byte(); // level
					recv.read_short(); // closeness
					recv.read_byte(); // fullness
					recv.read_long(); // expiration time
					recv.read_short(); // pet attribute
					recv.read_short(); // pet skill
					recv.read_int(); // remaining life
					recv.read_short(); // attribute
					return;
				}

				if (type != 1) // stackable item
				{
					recv.read_short(); // quantity
					recv.read_string(); // owner
					recv.read_short(); // item flags

					// Throwing stars and bullets carry a constant block
					if (item_id / 10000 == 207 || item_id / 10000 == 233)
						recv.skip(8); // writeInt(2) plus four constant bytes

					return;
				}

				// Equip
				recv.read_byte(); // upgrade slots
				recv.read_byte(); // level

				// str, dex, int, luk, hp, mp, watk, matk, wdef, mdef,
				// accuracy, avoid, hands, speed, jump
				for (int i = 0; i < 15; i++)
					recv.read_short();

				recv.read_string(); // owner
				recv.read_short(); // item flags

				if (cash)
				{
					recv.skip(10); // cash equips carry ten constant bytes
				}
				else
				{
					recv.read_byte(); // always 0
					recv.read_byte(); // item level
					recv.read_int(); // growth exp
					recv.read_int(); // vicious hammer count
					recv.read_long(); // always 0
				}

				recv.read_long(); // getTime(-2)
				recv.read_int(); // always -1
			}

			// One stock entry of a player shop or hired merchant item list
			void consume_shop_item(InPacket& recv)
			{
				recv.read_short(); // bundles
				recv.read_short(); // quantity
				recv.read_int(); // price
				consume_item_info(recv);
			}

			// A byte counted list of stock entries
			void consume_shop_item_list(InPacket& recv)
			{
				int32_t count = recv.read_byte();

				for (int32_t i = 0; i < count; i++)
					consume_shop_item(recv);
			}

			// The visitors of a mini room: every present visitor is introduced
			// by its slot number, an absent one by the 0xFF terminator
			void consume_room_visitors(InPacket& recv)
			{
				int8_t slot = recv.read_byte();

				while (slot != -1)
				{
					consume_char_look(recv);
					recv.read_string(); // visitor name
					slot = recv.read_byte();
				}
			}

			// One item description as written by PacketCreator.addCashItemInformation
			// (PacketCreator.java:6951). The gift form drops the account and serial
			// fields, appends the message and carries no expiration.
			void consume_cash_item_info(InPacket& recv, bool gift)
			{
				recv.read_long(); // pet id, ring id or cash id

				if (!gift)
				{
					recv.read_int(); // account id
					recv.read_int(); // always 0
				}

				recv.read_int(); // item id

				if (!gift)
				{
					recv.read_int(); // serial number
					recv.read_short(); // quantity
				}

				recv.read_padded_string(13); // gift from

				if (gift)
				{
					recv.read_padded_string(73); // gift message
					return;
				}

				recv.read_long(); // expiration time
				recv.read_long(); // always 0
			}

			// One MTS listing, as written by PacketCreator.sendMTS,
			// notYetSoldInv and transferInventory (PacketCreator.java:5452). All
			// three write the same entry.
			void consume_mts_listing(InPacket& recv)
			{
				consume_item_info(recv);
				recv.read_int(); // MTS item id
				recv.read_int(); // taxes
				recv.read_int(); // price
				recv.read_int(); // always 0
				recv.read_long(); // listing end time
				recv.read_string(); // seller account name
				recv.read_string(); // seller character name
				recv.skip(28); // constant padding
			}

			// Try a body shape on a copy of the packet, which shares the payload
			// but keeps its own read position: when the shape consumes the payload
			// exactly, the real packet adopts that position. Used where the encoder
			// picks a shape from state the payload itself does not carry.
			bool adopt_shape(InPacket& recv, void (*body)(InPacket&))
			{
				InPacket probe = recv;

				try
				{
					body(probe);
				}
				catch (const PacketError&)
				{
					return false;
				}

				if (probe.length() != 0)
					return false;

				recv = probe;
				return true;
			}

			// Parse a body which has to consume the rest of the payload, so that a
			// payload matching no known shape surfaces as a parse error instead of
			// being read only in part
			void parse_exact(InPacket& recv, void (*body)(InPacket&))
			{
				body(recv);

				if (recv.length() != 0)
					throw PacketError("Payload matches no known layout of this opcode");
			}

			// ---------------------------------------------------------------
			// 314 PLAYER_INTERACTION - PacketCreator.java:3143 - :3318, :4655 -
			// :4936 (player shop, trade, mini game and hired merchant rooms) and
			// PacketCreator.shopErrorMessage (PacketCreator.java:6761)
			// ---------------------------------------------------------------

			// PlayerInteractionHandler.Action (PlayerInteractionHandler.java:66).
			// Only the codes this server writes are listed.
			enum Action : uint8_t
			{
				INVITE = 0x02,
				VISIT = 0x04,
				ROOM = 0x05,
				CHAT = 0x06,
				EXIT = 0x0A,
				SET_ITEMS = 0x0F,
				SET_MESO = 0x10,
				CONFIRM = 0x11,
				UPDATE_MERCHANT = 0x19,
				UPDATE_PLAYERSHOP = 0x1A,
				REAL_CLOSE_MERCHANT = 0x2A,
				VIEW_VISITORS = 0x2E,
				VIEW_BLACKLIST = 0x2F,
				REQUEST_TIE = 0x32,
				ANSWER_TIE = 0x33,
				READY = 0x3A,
				UN_READY = 0x3B,
				START = 0x3D,
				GET_RESULT = 0x3E,
				SKIP = 0x3F,
				MOVE_OMOK = 0x40,
				SELECT_CARD = 0x44
			};

			// The hired merchant item list (PacketCreator.getHiredMerchant) holds
			// one extra byte in front of the entries when the shop is empty
			void consume_hired_merchant_item_list(InPacket& recv)
			{
				int32_t count = recv.read_byte();

				for (int32_t i = 0; i < count; i++)
					consume_shop_item(recv);

				if (count == 0)
					recv.read_byte(); // always 0
			}

			// The description block every hired merchant view ends with
			// (PacketCreator.getHiredMerchant)
			void consume_hired_merchant_room_tail(InPacket& recv)
			{
				recv.read_string(); // description
				recv.read_byte(); // slot count, 0x10 for most shops
				recv.read_int(); // mesos of the receiving character
				consume_hired_merchant_item_list(recv);
			}

			// The fields only the owner of the hired merchant room receives,
			// written in front of the description block by PacketCreator.getHiredMerchant
			void consume_hired_merchant_owner_header(InPacket& recv)
			{
				recv.read_short(); // always 0
				recv.read_short(); // time the merchant has been open
				recv.read_byte(); // 1 on the first view of the room, 0 afterwards

				int32_t sold = recv.read_byte();

				for (int32_t i = 0; i < sold; i++)
				{
					recv.read_int(); // sold item id
					recv.read_short(); // quantity
					recv.read_int(); // mesos
					recv.read_string(); // buyer name
				}

				recv.read_int(); // merchant mesos held for the owner
			}

			void consume_hired_merchant_owner_tail(InPacket& recv)
			{
				consume_hired_merchant_owner_header(recv);
				consume_hired_merchant_room_tail(recv);
			}

			// Room type 5: PacketCreator.getHiredMerchant
			void parse_interaction_hired_merchant(InPacket& recv)
			{
				recv.read_byte(); // always 4
				recv.read_short(); // the visitor slot this client occupies, plus one
				recv.read_int(); // hired merchant item id
				recv.read_string(); // "Hired Merchant"

				consume_room_visitors(recv);

				int32_t messages = recv.read_short();

				for (int32_t i = 0; i < messages; i++)
				{
					recv.read_string(); // message left for the owner
					recv.read_byte(); // message type
				}

				recv.read_string(); // owner name

				// The owner block and the description block differ in shape and
				// nothing in the payload tells them apart, so take the one which
				// consumes the packet exactly
				if (!adopt_shape(recv, consume_hired_merchant_owner_tail))
					parse_exact(recv, consume_hired_merchant_room_tail);
			}

			// Room type 4: PacketCreator.getPlayerShop
			void parse_interaction_player_shop(InPacket& recv)
			{
				recv.read_byte(); // always 4

				// 0 when the receiving client owns the shop, 1 when it visits
				bool visitor = recv.read_byte() != 0;

				if (visitor)
				{
					recv.read_byte(); // always 0
				}
				else
				{
					int32_t sold = recv.read_byte();

					for (int32_t i = 0; i < sold; i++)
					{
						recv.read_int(); // sold item id
						recv.read_short(); // quantity
						recv.read_int(); // mesos
						recv.read_string(); // buyer name
					}
				}

				consume_char_look(recv); // shop owner
				recv.read_string(); // owner name
				consume_room_visitors(recv);

				recv.read_string(); // description
				recv.read_byte(); // slot count, 0x10 for most shops

				consume_shop_item_list(recv);
			}

			// Room type 3: PacketCreator.getTradeStart
			void parse_interaction_trade(InPacket& recv)
			{
				recv.read_byte(); // always 2
				int8_t number = recv.read_byte(); // side of the trade, 1 or 2

				if (number == 1)
				{
					recv.read_byte(); // always 0
					consume_char_look(recv); // partner
					recv.read_string(); // partner name
				}

				recv.read_byte(); // the receiving client's side
				consume_char_look(recv);
				recv.read_string(); // the receiving client's name
				recv.read_byte(); // always 0xFF
			}

			// Room types 1 and 2: PacketCreator.getMiniGame and getMatchCard
			void parse_interaction_game_room(InPacket& recv)
			{
				recv.read_byte(); // 0 for the omok room, 2 for the match card room
				recv.read_bool(); // set when the receiving client visits
				recv.read_byte(); // always 0

				consume_char_look(recv); // room owner
				recv.read_string(); // owner name
				consume_room_visitors(recv);

				// Every player of the room is introduced by a byte (0 for the
				// owner, 1 for the visitor) followed by the game type and the
				// four score fields, until the 0xFF byte ends the board
				int8_t player = recv.read_byte();

				while (player != -1)
				{
					for (int i = 0; i < 5; i++)
						recv.read_int(); // game type, wins, ties, losses, points

					player = recv.read_byte();
				}

				recv.read_string(); // description
				recv.read_byte(); // piece
				recv.read_byte(); // always 0
			}

			void parse_interaction_room(InPacket& recv)
			{
				// Room type: 0 error and maintenance notices, 1 omok, 2 match
				// card, 3 trade, 4 player shop, 5 hired merchant
				int8_t room = recv.read_byte();

				switch (room)
				{
				case 0:
					// getMiniRoomError and hiredMerchantMaintenanceMessage
					recv.read_byte(); // error or maintenance code
					break;
				case 1:
				case 2:
					parse_interaction_game_room(recv);
					break;
				case 3:
					parse_interaction_trade(recv);
					break;
				case 4:
					parse_interaction_player_shop(recv);
					break;
				case 5:
					parse_interaction_hired_merchant(recv);
					break;
				default:
					// No room type is written with this value
					break;
				}
			}

			// VISIT is written by getPlayerShopNewVisitor, getTradePartnerAdd,
			// getMiniGameNewVisitor, getMatchCardNewVisitor and
			// hiredMerchantVisitorAdd; only the two mini game visitors append
			// their five score fields to the name.
			void parse_interaction_visit(InPacket& recv)
			{
				recv.read_byte(); // slot
				consume_char_look(recv);
				recv.read_string(); // name

				if (recv.length() > 0)
				{
					for (int i = 0; i < 5; i++)
						recv.read_int(); // game type, wins, ties, losses, points
				}
			}

			// EXIT is written by getPlayerShopRemoveVisitor (a short slot when it
			// is not zero), getTradeResult, getMiniGameRemoveVisitor,
			// getMiniGameClose, hiredMerchantVisitorLeave (a byte slot),
			// leaveHiredMerchant and shopErrorMessage: never more than two
			// trailing bytes, a shorter form carries less.
			void parse_interaction_exit(InPacket& recv)
			{
				if (recv.length() > 0)
					recv.read_byte();

				if (recv.length() > 0)
					recv.read_byte();
			}

			void parse_player_interaction(InPacket& recv)
			{
				uint8_t action = static_cast<uint8_t>(recv.read_byte());

				switch (action)
				{
				case INVITE:
					// tradeInvite: the constant marks a trade invitation
					recv.read_byte();
					recv.read_string(); // invited character
					recv.skip(4); // constant trade window bytes
					break;
				case VISIT:
					parse_interaction_visit(recv);
					break;
				case ROOM:
					parse_interaction_room(recv);
					break;
				case CHAT:
					// getPlayerShopChat, getTradeChat and hiredMerchantChat all
					// write the constant CHAT_THING code in front of the text
					recv.read_byte(); // CHAT_THING, always 8
					recv.read_byte(); // slot for shops, 0/1 for trades
					recv.read_string(); // the chat line
					break;
				case EXIT:
					parse_interaction_exit(recv);
					break;
				case SET_ITEMS:
					// getTradeItemAdd
					recv.read_byte(); // slot number
					recv.read_byte(); // item position
					consume_item_info(recv);
					break;
				case SET_MESO:
					// getTradeMesoSet
					recv.read_byte(); // slot number
					recv.read_int(); // mesos
					break;
				case CONFIRM:
					// getTradeConfirmation
					break;
				case UPDATE_MERCHANT:
					// getPlayerShopItemUpdate writes a byte counted item list,
					// updateHiredMerchant an int meso amount in front of it and
					// both are sent, so take the shape which fits the payload
					if (!adopt_shape(recv, consume_shop_item_list))
					{
						recv.read_int(); // merchant mesos
						parse_exact(recv, consume_shop_item_list);
					}
					break;
				case UPDATE_PLAYERSHOP:
					// getPlayerShopOwnerUpdate
					recv.read_byte(); // position
					recv.read_short(); // quantity
					recv.read_string(); // buyer name
					break;
				case REAL_CLOSE_MERCHANT:
					// hiredMerchantOwnerLeave and
					// hiredMerchantOwnerMaintenanceLeave
					recv.read_byte(); // 0 on a normal leave, 5 on maintenance
					break;
				case VIEW_VISITORS:
					// viewMerchantVisitorHistory
					{
						int32_t visits = recv.read_short();

						for (int32_t i = 0; i < visits; i++)
						{
							recv.read_string(); // visitor name
							recv.read_int(); // visit duration
						}
					}
					break;
				case VIEW_BLACKLIST:
					// viewMerchantBlacklist
					{
						int32_t names = recv.read_short();

						for (int32_t i = 0; i < names; i++)
							recv.read_string(); // blacklisted name
					}
					break;
				case REQUEST_TIE:
				case ANSWER_TIE:
				case READY:
				case UN_READY:
					// getMiniGameRequestTie, getMiniGameDenyTie, getMiniGameReady
					// and getMiniGameUnReady carry nothing else
					break;
				case START:
					// getMiniGameStart writes the losing side only, while
					// getMatchCardStart appends the card table
					recv.read_byte(); // losing side

					if (recv.length() > 0)
					{
						int32_t cards = recv.read_byte(); // 12, 20 or 30 cards

						for (int32_t i = 0; i < cards; i++)
							recv.read_int(); // card id
					}
					break;
				case GET_RESULT:
					// getMiniGameResult
					{
						int8_t type = recv.read_byte(); // 0, 1 or 2
						recv.read_bool(); // whether the visitor won

						if (type == 1)
						{
							recv.read_byte(); // always 0
							recv.read_short(); // always 0
							recv.skip(9 * 4); // both score blocks
							recv.read_byte(); // always 0
						}
						else
						{
							recv.skip(10 * 4); // both score blocks
						}
					}
					break;
				case SKIP:
					// getMiniGameSkipOwner writes the byte form, while
					// getMiniGameSkipVisitor writes the action as a short
					recv.read_byte();
					break;
				case MOVE_OMOK:
					// getMiniGameMoveOmok
					recv.read_int(); // first coordinate
					recv.read_int(); // second coordinate
					recv.read_byte(); // third coordinate
					break;
				case SELECT_CARD:
					// getMatchCardSelect
					{
						int8_t turn = recv.read_byte();

						if (turn == 1)
						{
							recv.read_byte(); // slot
						}
						else if (turn == 0)
						{
							recv.read_byte(); // slot
							recv.read_byte(); // first slot
							recv.read_byte(); // type
						}
					}
					break;
				default:
					// No encoder of this server writes any other action, so the
					// payload stays as it is and the log reports the leftovers
					break;
				}
			}

			// ---------------------------------------------------------------
			// 315 - 318 TOURNAMENT - PacketCreator.Tournament__Tournament,
			// Tournament__MatchTable, Tournament__SetPrize and Tournament__UEW
			// (PacketCreator.java:7463 - :7499)
			// ---------------------------------------------------------------

			void parse_tournament(InPacket& recv)
			{
				recv.read_byte(); // state
				recv.read_byte(); // sub state
			}

			void parse_tournament_match_table(InPacket& recv)
			{
				// Tournament__MatchTable opens the modal without any field
				(void)recv;
			}

			void parse_tournament_set_prize(InPacket& recv)
			{
				recv.read_byte(); // 0 on failure, 1 on success
				bool has_prize = recv.read_bool();

				if (has_prize)
				{
					recv.read_int(); // first prize item id
					recv.read_int(); // second prize item id
				}
			}

			void parse_tournament_uew(InPacket& recv)
			{
				recv.read_byte(); // state, a bit flag
			}

			// ---------------------------------------------------------------
			// 322 PARCEL - PacketCreator.removeItemFromDuey,
			// sendDueyParcelReceived, sendDueyParcelNotification and sendDuey
			// (PacketCreator.java:6525 - :6583)
			// ---------------------------------------------------------------

			void parse_parcel(InPacket& recv)
			{
				uint8_t operation = static_cast<uint8_t>(recv.read_byte());

				switch (operation)
				{
				case 0x17: // removeItemFromDuey
					recv.read_int(); // package id
					recv.read_byte(); // 3 when removed, 4 when kept in the list
					break;
				case 0x19: // sendDueyParcelReceived
					recv.read_string(); // sender
					recv.read_bool(); // quick delivery
					break;
				case 0x1B: // sendDueyParcelNotification
					recv.read_bool(); // quick delivery
					break;
				case 0x08: // sendDuey, the package list
					{
						recv.read_byte(); // always 0
						int32_t packages = recv.read_byte();

						for (int32_t i = 0; i < packages; i++)
						{
							recv.read_int(); // package id
							recv.read_padded_string(13); // sender
							recv.read_int(); // mesos
							recv.read_long(); // sent time
							recv.read_int(); // 1 when a message follows
							recv.skip(200); // fixed length message field
							recv.read_byte(); // always 0

							if (recv.read_bool())
								consume_item_info(recv);
						}

						recv.read_byte(); // always 0
					}
					break;
				default:
					// Every other duey operation is the byte on its own
					break;
				}
			}

			// ---------------------------------------------------------------
			// 324 QUERY_CASH_RESULT - PacketCreator.showCash
			// (PacketCreator.java:5714)
			// ---------------------------------------------------------------

			void parse_query_cash_result(InPacket& recv)
			{
				recv.read_int(); // nx credit
				recv.read_int(); // maple points
				recv.read_int(); // prepaid nx
			}

			// ---------------------------------------------------------------
			// 325 CASHSHOP_OPERATION - PacketCreator.java:5544, :5579, :5696,
			// :6474, :6489, :6978, :6998, :7007, :7069, :7076, :7095, :7108,
			// :7120, :7130, :7139, :7148, :7158, :7165 and :7173
			// ---------------------------------------------------------------

			void parse_cashshop_operation(InPacket& recv)
			{
				uint8_t operation = static_cast<uint8_t>(recv.read_byte());

				switch (operation)
				{
				case 0x4B: // showCashInventory
					{
						int32_t items = recv.read_short();

						for (int32_t i = 0; i < items; i++)
							consume_cash_item_info(recv, false);

						recv.read_short(); // storage slots
						recv.read_short(); // character slots
					}
					break;
				case 0x4D: // showGifts
					{
						int32_t gifts = recv.read_short();

						for (int32_t i = 0; i < gifts; i++)
							consume_cash_item_info(recv, true);
					}
					break;
				case 0x4F: // showWishList, loaded
				case 0x55: // showWishList, updated
					recv.skip(10 * 4); // always ten wish list slots
					break;
				case 0x57: // showBoughtCashItem
				case 0x6A: // putIntoCashInventory
				case 0x9E: // showNameChangeSuccess
				case 0xA0: // showWorldTransferSuccess
					consume_cash_item_info(recv, false);
					break;
				case 0x59: // showCouponRedeemedItems
					{
						int32_t cash_items = recv.read_byte();

						for (int32_t i = 0; i < cash_items; i++)
							consume_cash_item_info(recv, false);

						recv.read_int(); // maple points

						int32_t items = recv.read_int();

						for (int32_t i = 0; i < items; i++)
						{
							recv.read_short(); // quantity
							recv.read_short(); // always 0x1F
							recv.read_int(); // item id
						}

						recv.read_int(); // mesos
					}
					break;
				case 0x5C: // showCashShopMessage
					recv.read_byte(); // message code
					break;
				case 0x5E: // showGiftSucceed
					recv.read_string(); // recipient
					recv.read_int(); // item id
					recv.read_short(); // count
					recv.read_int(); // price
					break;
				case 0x60: // showBoughtInventorySlots
					recv.read_byte(); // inventory type
					recv.read_short(); // slots
					break;
				case 0x62: // showBoughtStorageSlots
				case 0x64: // showBoughtCharacterSlot
					recv.read_short(); // slots
					break;
				case 0x68: // takeFromCashInventory
					recv.read_short(); // cash inventory position
					consume_item_info(recv);
					break;
				case 0x6C: // deleteCashItem
				case 0x85: // refundCashItem, followed by the refunded maple points
					{
						recv.read_long(); // cash id

						if (operation == 0x85)
							recv.read_int(); // maple points
					}
					break;
				case 0x87: // showBoughtCashRing
					consume_cash_item_info(recv, false);
					recv.read_string(); // recipient
					recv.read_int(); // ring item id
					recv.read_short(); // quantity, always 1
					break;
				case 0x89: // showBoughtCashPackage
					{
						int32_t items = recv.read_byte();

						for (int32_t i = 0; i < items; i++)
							consume_cash_item_info(recv, false);

						recv.read_short(); // always 0
					}
					break;
				case 0x8D: // showBoughtQuestItem
					recv.read_int(); // always 1
					recv.read_short(); // always 1
					recv.read_byte(); // always 0x0B
					recv.read_byte(); // always 0
					recv.read_int(); // item id
					break;
				default:
					// No such cash shop operation is written
					break;
				}
			}

			// ---------------------------------------------------------------
			// 328, 329, 331 CASHSHOP_CHECK_* - PacketCreator.sendNameTransferCheck,
			// sendNameTransferRules and sendWorldTransferRules
			// (PacketCreator.java:5528, :5557, :5571)
			// ---------------------------------------------------------------

			void parse_cashshop_check_name_change(InPacket& recv)
			{
				recv.read_string(); // the checked name
				recv.read_bool(); // set when the name is taken
			}

			void parse_cashshop_check_name_change_possible_result(InPacket& recv)
			{
				recv.read_int(); // always 0
				recv.read_byte(); // 0 when a name change is possible, an error code otherwise
				recv.read_int(); // always 0
			}

			void parse_cashshop_check_transfer_world_possible_result(InPacket& recv)
			{
				recv.read_int(); // always 0
				recv.read_byte(); // 0 when a world transfer is possible, an error code otherwise
				recv.read_int(); // always 0

				if (recv.read_bool()) // set when the error byte is 0
				{
					int32_t worlds = recv.read_int();

					for (int32_t i = 0; i < worlds; i++)
						recv.read_string(); // world name
				}
			}

			// ---------------------------------------------------------------
			// 333 CASHSHOP_CASH_ITEM_GACHAPON_RESULT -
			// PacketCreator.onCashItemGachaponOpenFailed and
			// onCashGachaponOpenSuccess (PacketCreator.java:6501 - :6517)
			// ---------------------------------------------------------------

			void parse_cashshop_cash_item_gachapon_result(InPacket& recv)
			{
				uint8_t mode = static_cast<uint8_t>(recv.read_byte());

				if (mode != 0xE5) // 0xE4 carries nothing else
					return;

				recv.read_long(); // box cash id
				recv.read_int(); // remaining boxes
				consume_cash_item_info(recv, false);
				recv.read_int(); // reward item id
				recv.read_byte(); // reward quantity
				recv.read_bool(); // jackpot
			}

			// ---------------------------------------------------------------
			// 336, 337 AUTO_HP_POT and AUTO_MP_POT -
			// PacketCreator.sendAutoHpPot and sendAutoMpPot
			// (PacketCreator.java:5781, :5787)
			// ---------------------------------------------------------------

			void parse_auto_hp_pot(InPacket& recv)
			{
				recv.read_int(); // item id
			}

			void parse_auto_mp_pot(InPacket& recv)
			{
				recv.read_int(); // item id
			}

			// ---------------------------------------------------------------
			// 341 - 343 SEND_TV, REMOVE_TV, ENABLE_TV - PacketCreator.sendTV,
			// removeTV and enableTV (PacketCreator.java:908 - :952)
			// ---------------------------------------------------------------

			void parse_send_tv(InPacket& recv)
			{
				// 3 when a partner is shown next to the character, 1 otherwise
				int8_t has_partner = recv.read_byte();

				recv.read_byte(); // tv type: 0 normal, 1 star, 2 heart
				consume_char_look(recv); // character on the tv
				recv.read_string(); // character name

				if (has_partner == 3)
					recv.read_string(); // partner name
				else
					recv.read_short(); // always 0

				// The five messages of the tv, the count itself is not sent
				// (UseCashItemHandler.java:332 always collects five)
				for (int i = 0; i < 5; i++)
					recv.read_string();

				recv.read_int(); // time limit

				if (has_partner == 3)
					consume_char_look(recv); // partner on the tv
			}

			void parse_remove_tv(InPacket& recv)
			{
				// removeTV clears the tv without any field
				(void)recv;
			}

			void parse_enable_tv(InPacket& recv)
			{
				recv.read_int(); // always 0
				recv.read_byte(); // always 0
			}

			// ---------------------------------------------------------------
			// 347, 348 MTS_OPERATION2 and MTS_OPERATION -
			// PacketCreator.showMTSCash and the MTS_OPERATION encoders
			// (PacketCreator.java:5606, :5448 - :5690)
			// ---------------------------------------------------------------

			void parse_mts_operation2(InPacket& recv)
			{
				recv.read_int(); // prepaid nx
				recv.read_int(); // maple points
			}

			void parse_mts_operation(InPacket& recv)
			{
				uint8_t operation = static_cast<uint8_t>(recv.read_byte());

				switch (operation)
				{
				case 0x15: // sendMTS, one page of the market
					{
						recv.read_int(); // always pages * 16
						int32_t listings = recv.read_int();
						recv.read_int(); // tab
						recv.read_int(); // item type
						recv.read_int(); // page
						recv.read_byte(); // always 1
						recv.read_byte(); // always 1

						for (int32_t i = 0; i < listings; i++)
							consume_mts_listing(recv);

						recv.read_byte(); // always 1
					}
					break;
				case 0x1D: // MTSConfirmSell
				case 0x33: // MTSConfirmBuy
					break;
				case 0x21: // transferInventory
					{
						int32_t listings = recv.read_int();

						for (int32_t i = 0; i < listings; i++)
							consume_mts_listing(recv);

						recv.read_byte(); // 0xD0 plus the listing count
						recv.skip(4); // constant {-1, -1, -1, 0}
					}
					break;
				case 0x23: // notYetSoldInv
					{
						int32_t listings = recv.read_int();

						for (int32_t i = 0; i < listings; i++)
							consume_mts_listing(recv);

						// An empty list carries one extra int instead
						if (listings == 0)
							recv.read_int(); // always 0
					}
					break;
				case 0x27: // MTSConfirmTransfer
					recv.read_int(); // quantity
					recv.read_int(); // inventory position
					break;
				case 0x34: // MTSFailBuy
					recv.read_byte(); // always 0x42
					break;
				case 0x3D: // MTSWantedListingOver
					recv.read_int(); // nx
					recv.read_int(); // items
					break;
				default:
					// No other MTS operation is written
					break;
				}
			}

			// ---------------------------------------------------------------
			// 349, 350 MAPLELIFE_RESULT and MAPLELIFE_ERROR -
			// PacketCreator.sendMapleLifeCharacterInfo, sendMapleLifeNameError
			// and sendMapleLifeError (PacketCreator.java:2631 - :2650)
			// ---------------------------------------------------------------

			void parse_maplelife_result(InPacket& recv)
			{
				int32_t mode = recv.read_int(); // 0 on the character info request

				if (mode == 2) // sendMapleLifeNameError
				{
					recv.read_int(); // always 3
					recv.read_byte(); // always 0
				}
			}

			void parse_maplelife_error(InPacket& recv)
			{
				recv.read_byte(); // always 0
				recv.read_int(); // error code
			}

			// ---------------------------------------------------------------
			// 354 VICIOUS_HAMMER - PacketCreator.sendHammerData and
			// sendHammerMessage (PacketCreator.java:6177, :6185)
			// ---------------------------------------------------------------

			void parse_vicious_hammer(InPacket& recv)
			{
				uint8_t mode = static_cast<uint8_t>(recv.read_byte());

				if (mode == 0x39) // sendHammerData
				{
					recv.read_int(); // always 0
					recv.read_int(); // hammers used
				}
				else // 0x3D, sendHammerMessage
				{
					recv.read_int(); // always 0
				}
			}

			// ---------------------------------------------------------------
			// 358 VEGA_SCROLL - PacketCreator.sendVegaScroll
			// (PacketCreator.java:7270)
			// ---------------------------------------------------------------

			void parse_vega_scroll(InPacket& recv)
			{
				recv.read_byte(); // result code
			}

			// ---------------------------------------------------------------
			// 4096 UPDATE_HPMPAALERT - PacketCreator.updateClientSettings
			// (PacketCreator.java:7534)
			// ---------------------------------------------------------------

			void parse_update_hpmpaalert(InPacket& recv)
			{
				recv.read_byte(); // hp alert threshold
				recv.read_byte(); // mp alert threshold
			}
		}

		void register_packets8()
		{
			add(ServerOpcode::PLAYER_INTERACTION, parse_player_interaction);
			add(ServerOpcode::TOURNAMENT, parse_tournament);
			add(ServerOpcode::TOURNAMENT_MATCH_TABLE, parse_tournament_match_table);
			add(ServerOpcode::TOURNAMENT_SET_PRIZE, parse_tournament_set_prize);
			add(ServerOpcode::TOURNAMENT_UEW, parse_tournament_uew);
			add(ServerOpcode::PARCEL, parse_parcel);
			add(ServerOpcode::QUERY_CASH_RESULT, parse_query_cash_result);
			add(ServerOpcode::CASHSHOP_OPERATION, parse_cashshop_operation);
			add(ServerOpcode::CASHSHOP_CHECK_NAME_CHANGE, parse_cashshop_check_name_change);
			add(ServerOpcode::CASHSHOP_CHECK_NAME_CHANGE_POSSIBLE_RESULT, parse_cashshop_check_name_change_possible_result);
			add(ServerOpcode::CASHSHOP_CHECK_TRANSFER_WORLD_POSSIBLE_RESULT, parse_cashshop_check_transfer_world_possible_result);
			add(ServerOpcode::CASHSHOP_CASH_ITEM_GACHAPON_RESULT, parse_cashshop_cash_item_gachapon_result);
			add(ServerOpcode::AUTO_HP_POT, parse_auto_hp_pot);
			add(ServerOpcode::AUTO_MP_POT, parse_auto_mp_pot);
			add(ServerOpcode::SEND_TV, parse_send_tv);
			add(ServerOpcode::REMOVE_TV, parse_remove_tv);
			add(ServerOpcode::ENABLE_TV, parse_enable_tv);
			add(ServerOpcode::MTS_OPERATION2, parse_mts_operation2);
			add(ServerOpcode::MTS_OPERATION, parse_mts_operation);
			add(ServerOpcode::MAPLELIFE_RESULT, parse_maplelife_result);
			add(ServerOpcode::MAPLELIFE_ERROR, parse_maplelife_error);
			add(ServerOpcode::VICIOUS_HAMMER, parse_vicious_hammer);
			add(ServerOpcode::VEGA_SCROLL, parse_vega_scroll);

			// Opcode 4096 is above PacketSwitch::NUM_HANDLERS (500) and above the
			// Unsupported table bound (UnsupportedPackets.cpp:24), so the client
			// never routes it here; the entry documents the layout in the meantime
			add(ServerOpcode::UPDATE_HPMPAALERT, parse_update_hpmpaalert);
		}
	}
}