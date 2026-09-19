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
		// 65 GUILD_OPERATION, 66 ALLIANCE_OPERATION, 67 SPAWN_PORTAL, 69 INCUBATOR_RESULT, 70 SHOP_SCANNER_RESULT, 71 SHOP_LINK_RESULT, 72 MARRIAGE_REQUEST, 73 MARRIAGE_RESULT, 74 WEDDING_GIFT_RESULT, 75 NOTIFY_MARRIED_PARTNER_MAP_TRANSFER, 76 CASH_PET_FOOD_RESULT, 79 BRIDLE_MOB_CATCH_FAIL, 81 IMITATED_NPC_DATA, 83 MONSTER_BOOK_SET_CARD, 84 MONSTER_BOOK_SET_COVER, 90 SESSION_VALUE, 94 FAMILY_CHART_RESULT, 95 FAMILY_INFO_RESULT, 96 FAMILY_RESULT, 97 FAMILY_JOIN_REQUEST, 98 FAMILY_JOIN_REQUEST_RESULT, 99 FAMILY_JOIN_ACCEPTED, 100 FAMILY_PRIVILEGE_LIST, 101 FAMILY_REP_GAIN
		//
		// Each parser reads exactly the fields the server writes for its opcode, so a
		// server-side layout change surfaces as a PacketError instead of silence.
		// Guild, alliance, shop, player npc and marriage payloads start with a
		// sub-type byte and continue with a shape which depends on it, so those
		// parsers dispatch on the sub-type exactly like the encoder does.
		namespace
		{
			// Consume one item description as written by PacketCreator.addItemInfo
			// with zeroPosition = true: no slot field, then the item type, the id,
			// the cash flag, an optional unique id, the expiration and the pet,
			// equip or stackable detail of the item.
			void consume_item_block(InPacket& recv)
			{
				int8_t type = recv.read_byte();
				int32_t item_id = recv.read_int();
				bool cash = recv.read_bool();

				if (cash)
					recv.skip(8); // unique id: pet id, ring id or cash id

				recv.read_long(); // expiration time

				// Pets are recognised by their item id (ItemConstants.isPet)
				if (item_id / 1000 == 5000)
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

				if (type == 1) // equip (InventoryType.EQUIP)
				{
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
						recv.skip(10); // cash equips carry ten constant bytes instead
					}
					else
					{
						recv.read_byte(); // always 0
						recv.read_byte(); // item level
						recv.read_int(); // item exp
						recv.read_int(); // vicious hammer
						recv.read_long(); // always 0
					}

					recv.read_long(); // getTime(-2)
					recv.read_int(); // -1
				}
				else
				{
					recv.read_short(); // quantity
					recv.read_string(); // owner
					recv.read_short(); // item flags

					// Throwing stars and bullets add a constant block
					if (item_id / 10000 == 207 || item_id / 10000 == 233)
						recv.skip(8); // writeInt(2) plus four constant bytes
				}
			}

			// Consume a byte counted list of item names, as written by
			// WeddingPackets.onWeddingGiftResult
			void consume_name_list(InPacket& recv)
			{
				int8_t count = recv.read_byte();

				for (int8_t i = 0; i < count; i++)
					recv.read_string();
			}

			// Consume a byte counted list of item descriptions
			void consume_item_list(InPacket& recv)
			{
				int8_t count = recv.read_byte();

				for (int8_t i = 0; i < count; i++)
					consume_item_block(recv);
			}

			// Consume the guild description of GuildPackets.getGuildInfo, which
			// showGuildInfo and the alliance encoders embed: id, name, the five rank
			// titles, the member ids, one block per member, capacity, emblem and the
			// guild notice.
			void consume_guild_block(InPacket& recv)
			{
				recv.read_int(); // guild id
				recv.read_string(); // guild name

				for (int i = 0; i < 5; i++)
					recv.read_string(); // rank titles 1 - 5

				int8_t member_count = recv.read_byte();

				for (int8_t i = 0; i < member_count; i++)
					recv.read_int(); // character id of each member

				for (int8_t i = 0; i < member_count; i++)
				{
					recv.read_padded_string(13); // name
					recv.read_int(); // job
					recv.read_int(); // level
					recv.read_int(); // guild rank
					recv.read_int(); // online
					recv.read_int(); // guild signature
					recv.read_int(); // alliance rank
				}

				recv.read_int(); // capacity
				recv.read_short(); // emblem background
				recv.read_byte(); // emblem background colour
				recv.read_short(); // emblem
				recv.read_byte(); // emblem colour
				recv.read_string(); // notice
				recv.read_int(); // guild points
				recv.read_int(); // alliance id
			}

			// Consume one entry of PacketCreator.addPedigreeEntry
			void consume_pedigree_entry(InPacket& recv)
			{
				recv.read_int(); // character id
				recv.read_int(); // parent id
				recv.read_short(); // job
				recv.read_byte(); // level
				recv.read_byte(); // online
				recv.read_int(); // current reputation
				recv.read_int(); // total reputation
				recv.read_int(); // reputation given to the senior
				recv.read_int(); // reputation gained today
				recv.read_int(); // channel, -1 when away, 0 when offline
				recv.read_int(); // minutes online
				recv.read_string(); // name
			}

			// Consume the (position, item id) list of a player npc as written by
			// PacketCreator.getPlayerNPC, terminated by 0xFF
			void consume_npc_equips(InPacket& recv)
			{
				int8_t position = recv.read_byte();

				while (position != -1)
				{
					recv.read_int(); // item id
					position = recv.read_byte();
				}
			}

			// 65 GUILD_OPERATION - GuildPackets.showGuildInfo, guildMemberOnline,
			// guildInvite, createGuildMessage, genericGuildMessage,
			// responseGuildMessage, newGuildMember, memberLeft, changeRank,
			// guildNotice, guildMemberLevelJobUpdate, rankTitleChange,
			// guildDisband, guildQuestWaitingNotice, guildEmblemChange,
			// guildCapacityChange, showGuildRanks / showPlayerRanks and updateGP
			void parse_guild_operation(InPacket& recv)
			{
				int8_t sub = recv.read_byte();

				switch (sub)
				{
				case 0x03: // createGuildMessage
					recv.read_int(); // always 0
					recv.read_string(); // master name
					recv.read_string(); // guild name
					break;
				case 0x05: // guildInvite
					recv.read_int(); // guild id
					recv.read_string(); // character name
					break;
				case 0x1A: // showGuildInfo, the flag decides whether a guild follows
					if (recv.read_byte() == 1) // in guild
						consume_guild_block(recv);
					break;
				case 0x27: // newGuildMember
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_padded_string(13); // name

					for (int i = 0; i < 6; i++)
						recv.read_int(); // job, level, rank, online, signature, alliance rank
					break;
				case 0x2C: // memberLeft
				case 0x2F: // memberLeft, expelled
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_string(); // name
					break;
				case 0x32: // guildDisband
					recv.read_int(); // guild id
					recv.read_byte(); // always 1
					break;
				case 0x35: // responseGuildMessage, name bearing response code
				case 0x36: // responseGuildMessage, GuildResponse.MANAGING_INVITE
				case 0x37: // responseGuildMessage, GuildResponse.DENIED_INVITE
					recv.read_string(); // target name
					break;
				case 0x3A: // guildCapacityChange
					recv.read_int(); // guild id
					recv.read_byte(); // capacity
					break;
				case 0x3C: // guildMemberLevelJobUpdate
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_int(); // level
					recv.read_int(); // job
					break;
				case 0x3D: // guildMemberOnline
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_byte(); // online
					break;
				case 0x3E: // rankTitleChange
					recv.read_int(); // guild id

					for (int i = 0; i < 5; i++)
						recv.read_string(); // rank titles 1 - 5
					break;
				case 0x40: // changeRank
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_byte(); // new rank
					break;
				case 0x42: // guildEmblemChange
					recv.read_int(); // guild id
					recv.read_short(); // background
					recv.read_byte(); // background colour
					recv.read_short(); // emblem
					recv.read_byte(); // emblem colour
					break;
				case 0x44: // guildNotice
					recv.read_int(); // guild id
					recv.read_string(); // notice
					break;
				case 0x48: // updateGP
					recv.read_int(); // guild id
					recv.read_int(); // guild points
					break;
				case 0x49: // showGuildRanks and showPlayerRanks
				{
					recv.read_int(); // npc id
					int32_t count = recv.read_int(); // number of ranked entries

					for (int32_t i = 0; i < count; i++)
					{
						recv.read_string(); // guild or character name

						for (int j = 0; j < 5; j++)
							recv.read_int(); // gp and emblem, or rank and zeroes
					}
					break;
				}
				case 0x4C: // guildQuestWaitingNotice
					recv.read_byte(); // channel - 1
					recv.read_byte(); // waiting position
					break;
				default:
					// The remaining sub-types are the plain response codes of
					// genericGuildMessage, which carry no further fields
					break;
				}
			}

			// 66 ALLIANCE_OPERATION - GuildPackets.getAllianceInfo,
			// updateAllianceInfo, getGuildAlliances, addGuildToAlliance,
			// allianceMemberOnline, allianceNotice, changeAllianceRankTitle,
			// updateAllianceJobLevel, removeGuildFromAlliance, disbandAlliance,
			// allianceInvite, sendShowInfo, sendInvitation, sendChangeGuild,
			// sendChangeLeader and sendChangeRank
			void parse_alliance_operation(InPacket& recv)
			{
				int8_t sub = recv.read_byte();

				switch (sub)
				{
				case 0x02: // sendShowInfo
					recv.read_int(); // alliance id
					recv.read_int(); // player id
					break;
				case 0x03: // allianceInvite
					recv.read_int(); // alliance id
					recv.read_string(); // character name
					recv.read_short(); // always 0
					break;
				case 0x05: // sendInvitation
					recv.read_int(); // alliance id
					recv.read_int(); // player id
					recv.read_string(); // guild name
					break;
				case 0x07: // sendChangeGuild
					recv.read_int(); // alliance id
					recv.read_int(); // guild id
					recv.read_int(); // player id
					recv.read_byte(); // option
					break;
				case 0x08: // sendChangeLeader
					recv.read_int(); // alliance id
					recv.read_int(); // player id
					recv.read_int(); // victim
					break;
				case 0x09: // sendChangeRank
					recv.read_int(); // alliance id
					recv.read_int(); // player id
					recv.read_int(); // value
					recv.read_int(); // rank, written as an int by the server
					break;
				case 0x0C: // getAllianceInfo
				{
					recv.read_byte(); // always 1
					recv.read_int(); // alliance id
					recv.read_string(); // name

					for (int i = 0; i < 5; i++)
						recv.read_string(); // rank titles 1 - 5

					int8_t guild_count = recv.read_byte();
					recv.read_int(); // capacity

					for (int8_t i = 0; i < guild_count; i++)
						recv.read_int(); // guild id

					recv.read_string(); // notice
					break;
				}
				case 0x0D: // getGuildAlliances
				{
					int8_t guild_count = recv.read_byte();

					for (int8_t i = 0; i < guild_count; i++)
						consume_guild_block(recv);
					break;
				}
				case 0x0E: // allianceMemberOnline
					recv.read_int(); // alliance id
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_byte(); // online
					break;
				case 0x0F: // updateAllianceInfo
				{
					recv.read_int(); // alliance id
					recv.read_string(); // name

					for (int i = 0; i < 5; i++)
						recv.read_string(); // rank titles 1 - 5

					int8_t guild_count = recv.read_byte();

					for (int8_t i = 0; i < guild_count; i++)
						recv.read_int(); // guild id

					recv.read_int(); // capacity
					recv.read_short(); // always 0

					for (int8_t i = 0; i < guild_count; i++)
						consume_guild_block(recv);
					break;
				}
				case 0x10: // removeGuildFromAlliance
				{
					recv.read_int(); // alliance id
					recv.read_string(); // name

					for (int i = 0; i < 5; i++)
						recv.read_string(); // rank titles 1 - 5

					int8_t guild_count = recv.read_byte();

					for (int8_t i = 0; i < guild_count; i++)
						recv.read_int(); // guild id

					recv.read_int(); // capacity
					recv.read_string(); // notice
					recv.read_int(); // expelled guild id
					consume_guild_block(recv); // the expelled guild
					recv.read_byte(); // always 1
					break;
				}
				case 0x12: // addGuildToAlliance
				{
					recv.read_int(); // alliance id
					recv.read_string(); // name

					for (int i = 0; i < 5; i++)
						recv.read_string(); // rank titles 1 - 5

					int8_t guild_count = recv.read_byte();

					for (int8_t i = 0; i < guild_count; i++)
						recv.read_int(); // guild id

					recv.read_int(); // capacity
					recv.read_string(); // notice
					recv.read_int(); // new guild id
					consume_guild_block(recv); // the new guild
					break;
				}
				case 0x18: // updateAllianceJobLevel
					recv.read_int(); // alliance id
					recv.read_int(); // guild id
					recv.read_int(); // character id
					recv.read_int(); // level
					recv.read_int(); // job
					break;
				case 0x1A: // changeAllianceRankTitle
					recv.read_int(); // alliance id

					for (int i = 0; i < 5; i++)
						recv.read_string(); // rank titles 1 - 5
					break;
				case 0x1C: // allianceNotice
					recv.read_int(); // alliance id
					recv.read_string(); // notice
					break;
				case 0x1D: // disbandAlliance
					recv.read_int(); // alliance id
					break;
				default:
					break;
				}
			}

			// 67 SPAWN_PORTAL - PacketCreator.spawnPortal and the town branch of
			// PacketCreator.removeDoor, which sends the portal without a position
			void parse_spawn_portal(InPacket& recv)
			{
				recv.read_int(); // town id
				recv.read_int(); // target id

				// Only spawnPortal appends the position, removeDoor stops here
				if (recv.length() >= 4)
					recv.read_point(); // portal position
			}

			// 69 INCUBATOR_RESULT - PacketCreator.incubatorResult, six skipped bytes
			void parse_incubator_result(InPacket& recv)
			{
				recv.skip(6);
			}

			// 70 SHOP_SCANNER_RESULT - PacketCreator.owlOfMinerva (sub-type 6) and
			// PacketCreator.getOwlOpen (sub-type 7)
			void parse_shop_scanner_result(InPacket& recv)
			{
				int8_t sub = recv.read_byte();

				if (sub == 6) // owl of minerva search result
				{
					recv.read_int(); // always 0
					recv.read_int(); // searched item id

					int32_t count = recv.read_int(); // number of shops

					for (int32_t i = 0; i < count; i++)
					{
						recv.read_string(); // shop owner
						recv.read_int(); // map id of the shop
						recv.read_string(); // shop description
						recv.read_int(); // bundles
						recv.read_int(); // quantity
						recv.read_int(); // price
						recv.read_int(); // owner character id
						recv.read_byte(); // channel - 1

						// Only equips are followed by the item description
						if (recv.read_byte() == 1) // InventoryType.EQUIP
							consume_item_block(recv);
					}
				}
				else if (sub == 7) // owl leaderboards
				{
					int8_t count = recv.read_byte();

					for (int8_t i = 0; i < count; i++)
						recv.read_int(); // leaderboard entry
				}
			}

			// 71 SHOP_LINK_RESULT - PacketCreator.getOwlMessage
			void parse_shop_link_result(InPacket& recv)
			{
				recv.read_byte(); // result code, selects the message shown
			}

			// 72 MARRIAGE_REQUEST - WeddingPackets.onMarriageRequest (mode 0) and
			// WeddingPackets.sendWishList (mode 9, which carries no further field)
			void parse_marriage_request(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				if (mode == 0) // proposal
				{
					recv.read_string(); // name of the partner
					recv.read_int(); // character id of the partner
				}
			}

			// 73 MARRIAGE_RESULT - WeddingPackets.OnMarriageResult(marriageId, ...)
			// (mode 11), WeddingPackets.sendWeddingInvitation (mode 15) and
			// WeddingPackets.OnMarriageResult(msg) (mode 36 carries a message)
			void parse_marriage_result(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				switch (mode)
				{
				case 11: // engagement or wedding confirmed
					recv.read_int(); // marriage id
					recv.read_int(); // groom character id
					recv.read_int(); // bride character id
					recv.read_short(); // 1 = engaged, 3 = married
					recv.read_int(); // groom's ring
					recv.read_int(); // bride's ring
					recv.read_padded_string(13); // groom name
					recv.read_padded_string(13); // bride name
					break;
				case 15: // wedding invitation
					recv.read_string(); // groom name
					recv.read_string(); // bride name
					recv.read_short(); // wedding type
					break;
				case 36: // engagement accepted
					recv.read_byte(); // always 1
					recv.read_string(); // message
					break;
				default:
					// The other result codes carry only the mode byte
					break;
				}
			}

			// 74 WEDDING_GIFT_RESULT - WeddingPackets.onWeddingGiftResult
			void parse_wedding_gift_result(InPacket& recv)
			{
				int8_t mode = recv.read_byte();

				switch (mode)
				{
				case 0x09: // load the wedding registry
					consume_name_list(recv);
					break;
				case 0x0A: // load the bride's wishlist
				case 0x0F: // collect a gift
					recv.read_long(); // always 32
					consume_item_list(recv);
					break;
				case 0x0B: // send a gift
					consume_name_list(recv);
					recv.read_long(); // always 32
					consume_item_list(recv);
					break;
				case 0x0C: // cannot give more than one present per wishlist entry
				case 0x0E: // failed to send the gift
				default:
					break;
				}
			}

			// 75 NOTIFY_MARRIED_PARTNER_MAP_TRANSFER -
			// WeddingPackets.OnNotifyWeddingPartnerTransfer
			void parse_notify_married_partner_map_transfer(InPacket& recv)
			{
				recv.read_int(); // map id the partner moved to
				recv.read_int(); // partner character id
			}

			// 76 CASH_PET_FOOD_RESULT - PacketCreator.petEatCashFoodFail
			void parse_cash_pet_food_result(InPacket& recv)
			{
				recv.read_byte(); // failure flag, always 1
			}

			// 79 BRIDLE_MOB_CATCH_FAIL - PacketCreator.catchMessage
			void parse_bridle_mob_catch_fail(InPacket& recv)
			{
				recv.read_byte(); // 1 = too strong, 2 = elemental rock
				recv.read_int(); // always 0
				recv.read_int(); // always 0
			}

			// 81 IMITATED_NPC_DATA - PacketCreator.getPlayerNPC (sub-type 1) and
			// PacketCreator.removePlayerNPC (sub-type 0)
			void parse_imitated_npc_data(InPacket& recv)
			{
				int8_t sub = recv.read_byte();

				if (sub == 0x01) // add or update a player npc
				{
					recv.read_int(); // npc script id
					recv.read_string(); // name
					recv.read_byte(); // gender
					recv.read_byte(); // skin
					recv.read_int(); // face
					recv.read_byte(); // always 0
					recv.read_int(); // hair

					consume_npc_equips(recv); // equipped items
					consume_npc_equips(recv); // cash overrides

					recv.read_int(); // cash weapon
					recv.read_int(); // always 0
					recv.read_int(); // always 0
					recv.read_int(); // always 0
				}
				else if (sub == 0x00) // remove a player npc
				{
					recv.read_int(); // object id
				}
			}

			// 83 MONSTER_BOOK_SET_CARD - PacketCreator.addCard
			void parse_monster_book_set_card(InPacket& recv)
			{
				recv.read_byte(); // 0 when the book was full, 1 when the card was added
				recv.read_int(); // card id
				recv.read_int(); // card level
			}

			// 84 MONSTER_BOOK_SET_COVER - PacketCreator.changeCover
			void parse_monster_book_set_cover(InPacket& recv)
			{
				recv.read_int(); // card id used as the cover
			}

			// 90 SESSION_VALUE - PacketCreator.getEnergy
			void parse_session_value(InPacket& recv)
			{
				recv.read_string(); // value name
				recv.read_string(); // value, written as a string
			}

			// 94 FAMILY_CHART_RESULT - PacketCreator.showPedigree
			void parse_family_chart_result(InPacket& recv)
			{
				recv.read_int(); // character id whose pedigree is shown

				int32_t entry_count = recv.read_int();

				for (int32_t i = 0; i < entry_count; i++)
					consume_pedigree_entry(recv);

				int32_t member_info_count = recv.read_int();

				// Every member info is a (marker, value) pair: -1 carries the total
				// member count, 0 the total senior count, any other marker is the
				// character id of a super junior
				for (int32_t i = 0; i < member_info_count; i++)
				{
					recv.read_int(); // marker
					recv.read_int(); // value
				}

				// The server keeps its entitlement loop disabled, so the count is
				// written as zero and no entry follows
				recv.read_int(); // entitlement count

				recv.read_short(); // 0 disables the add button, 2 enables it
			}

			// 95 FAMILY_INFO_RESULT - PacketCreator.getFamilyInfo and
			// PacketCreator.getEmptyFamilyInfo
			void parse_family_info_result(InPacket& recv)
			{
				recv.read_int(); // reputation left
				recv.read_int(); // total reputation
				recv.read_int(); // reputation gained today
				recv.read_short(); // juniors added
				recv.read_short(); // juniors allowed
				recv.read_short(); // unused
				recv.read_int(); // leader character id
				recv.read_string(); // family name
				recv.read_string(); // family message

				int32_t entitlement_count = recv.read_int();

				for (int32_t i = 0; i < entitlement_count; i++)
				{
					recv.read_int(); // entitlement id
					recv.read_int(); // times used
				}
			}

			// 96 FAMILY_RESULT - PacketCreator.sendFamilyMessage
			void parse_family_result(InPacket& recv)
			{
				recv.read_int(); // result type
				recv.read_int(); // mesos required
			}

			// 97 FAMILY_JOIN_REQUEST - PacketCreator.sendFamilyInvite
			void parse_family_join_request(InPacket& recv)
			{
				recv.read_int(); // character id of the invited player
				recv.read_string(); // inviter name
			}

			// 98 FAMILY_JOIN_REQUEST_RESULT - PacketCreator.sendFamilyJoinResponse
			void parse_family_join_request_result(InPacket& recv)
			{
				recv.read_byte(); // 1 when accepted, 0 when refused
				recv.read_string(); // name of the junior
			}

			// 99 FAMILY_JOIN_ACCEPTED - PacketCreator.getSeniorMessage
			void parse_family_join_accepted(InPacket& recv)
			{
				recv.read_string(); // senior name
				recv.read_int(); // always 0
			}

			// 100 FAMILY_PRIVILEGE_LIST - PacketCreator.loadFamily
			void parse_family_privilege_list(InPacket& recv)
			{
				int32_t count = recv.read_int(); // number of family entitlements

				for (int32_t i = 0; i < count; i++)
				{
					recv.read_byte(); // entitlement type
					recv.read_int(); // reputation cost
					recv.read_int(); // usage limit
					recv.read_string(); // name
					recv.read_string(); // description
				}
			}

			// 101 FAMILY_REP_GAIN - PacketCreator.sendGainRep
			void parse_family_rep_gain(InPacket& recv)
			{
				recv.read_int(); // reputation gained
				recv.read_string(); // name of the family member
			}
		}

		void register_packets2()
		{
			add(ServerOpcode::GUILD_OPERATION, parse_guild_operation);
			add(ServerOpcode::ALLIANCE_OPERATION, parse_alliance_operation);
			add(ServerOpcode::SPAWN_PORTAL, parse_spawn_portal);
			add(ServerOpcode::INCUBATOR_RESULT, parse_incubator_result);
			add(ServerOpcode::SHOP_SCANNER_RESULT, parse_shop_scanner_result);
			add(ServerOpcode::SHOP_LINK_RESULT, parse_shop_link_result);
			add(ServerOpcode::MARRIAGE_REQUEST, parse_marriage_request);
			add(ServerOpcode::MARRIAGE_RESULT, parse_marriage_result);
			add(ServerOpcode::WEDDING_GIFT_RESULT, parse_wedding_gift_result);
			add(ServerOpcode::NOTIFY_MARRIED_PARTNER_MAP_TRANSFER, parse_notify_married_partner_map_transfer);
			add(ServerOpcode::CASH_PET_FOOD_RESULT, parse_cash_pet_food_result);
			add(ServerOpcode::BRIDLE_MOB_CATCH_FAIL, parse_bridle_mob_catch_fail);
			add(ServerOpcode::IMITATED_NPC_DATA, parse_imitated_npc_data);
			add(ServerOpcode::MONSTER_BOOK_SET_CARD, parse_monster_book_set_card);
			add(ServerOpcode::MONSTER_BOOK_SET_COVER, parse_monster_book_set_cover);
			add(ServerOpcode::SESSION_VALUE, parse_session_value);
			add(ServerOpcode::FAMILY_CHART_RESULT, parse_family_chart_result);
			add(ServerOpcode::FAMILY_INFO_RESULT, parse_family_info_result);
			add(ServerOpcode::FAMILY_RESULT, parse_family_result);
			add(ServerOpcode::FAMILY_JOIN_REQUEST, parse_family_join_request);
			add(ServerOpcode::FAMILY_JOIN_REQUEST_RESULT, parse_family_join_request_result);
			add(ServerOpcode::FAMILY_JOIN_ACCEPTED, parse_family_join_accepted);
			add(ServerOpcode::FAMILY_PRIVILEGE_LIST, parse_family_privilege_list);
			add(ServerOpcode::FAMILY_REP_GAIN, parse_family_rep_gain);
		}
	}
}
