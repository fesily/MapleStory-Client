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
#include "TextResolver.h"

#include "ItemData.h"
#include "SkillData.h"

#include "../Gameplay/Stage.h"

#include "../Util/Misc.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace
{
	// The predefined NoLifeNx node of the file a data path starts with, the ones the
	// codes #f, #W and #B carry (nx.hpp declares every file the data is split over)
	nl::node nx_root(const std::string& name)
	{
		if (name == "Base") return nl::nx::Base;
		if (name == "Character") return nl::nx::Character;
		if (name == "Effect") return nl::nx::Effect;
		if (name == "Etc") return nl::nx::Etc;
		if (name == "Item") return nl::nx::Item;
		if (name == "Map") return nl::nx::Map;
		if (name == "Map001") return nl::nx::Map001;
		if (name == "Map002") return nl::nx::Map002;
		if (name == "Map2") return nl::nx::Map2;
		if (name == "Mob") return nl::nx::Mob;
		if (name == "Mob001") return nl::nx::Mob001;
		if (name == "Mob002") return nl::nx::Mob002;
		if (name == "Mob2") return nl::nx::Mob2;
		if (name == "Morph") return nl::nx::Morph;
		if (name == "Npc") return nl::nx::Npc;
		if (name == "Quest") return nl::nx::Quest;
		if (name == "Reactor") return nl::nx::Reactor;
		if (name == "Skill") return nl::nx::Skill;
		if (name == "Skill001") return nl::nx::Skill001;
		if (name == "Skill002") return nl::nx::Skill002;
		if (name == "Skill003") return nl::nx::Skill003;
		if (name == "Sound") return nl::nx::Sound;
		if (name == "Sound001") return nl::nx::Sound001;
		if (name == "Sound002") return nl::nx::Sound002;
		if (name == "Sound2") return nl::nx::Sound2;
		if (name == "String") return nl::nx::String;
		if (name == "TamingMob") return nl::nx::TamingMob;
		if (name == "UI") return nl::nx::UI;

		return nl::node();
	}
}

namespace ms
{
	std::string TextResolver::item_name(int32_t itemid) const
	{
		const ItemData& data = ItemData::get(itemid);

		return data.is_valid() ? data.get_name() : std::string();
	}

	Texture TextResolver::item_icon(int32_t itemid) const
	{
		const ItemData& data = ItemData::get(itemid);

		return data.is_valid() ? data.get_icon(false) : Texture();
	}

	Texture TextResolver::skill_icon(int32_t skillid) const
	{
		return SkillData::get(skillid).get_icon(SkillData::Icon::NORMAL);
	}

	Texture TextResolver::ui_image(const std::string& path) const
	{
		// The paths the codes carry start with the name of one of the files the
		// client's data is split over (NoLifeNx' predefined nodes)
		size_t slash = path.find('/');
		std::string rootname = path.substr(0, slash);

		nl::node node = nx_root(rootname);

		for (size_t pos = slash + 1; node && pos < path.size();)
		{
			size_t next = path.find('/', pos);

			node = node[path.substr(pos, next == std::string::npos ? std::string::npos : next - pos)];
			pos = next == std::string::npos ? path.size() : next + 1;
		}

		return Texture(node);
	}

	std::string TextResolver::mob_name(int32_t mobid) const
	{
		return nl::nx::String["Mob.img"][std::to_string(mobid)]["name"];
	}

	std::string TextResolver::npc_name(int32_t npcid) const
	{
		return nl::nx::String["Npc.img"][std::to_string(npcid)]["name"];
	}

	std::string TextResolver::map_name(int32_t mapid) const
	{
		return NxHelper::Map::get_map_info_by_id(mapid).name;
	}

	std::string TextResolver::skill_name(int32_t skillid) const
	{
		// Skill data is loaded on demand and only ever empty when the id is unknown
		return SkillData::get(skillid).get_name();
	}

	std::string TextResolver::quest_name(int32_t questid) const
	{
		return nl::nx::Quest["QuestInfo.img"][std::to_string(questid)]["name"];
	}

	std::string TextResolver::player_name() const
	{
		return Stage::get().get_player().get_name();
	}

	int32_t TextResolver::item_count(int32_t itemid) const
	{
		return Stage::get().get_player().get_inventory().get_total_item_count(itemid);
	}
}
