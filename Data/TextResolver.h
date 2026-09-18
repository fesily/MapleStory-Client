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
#pragma once

#include "../Graphics/TextAnalyzer.h"

#include "../Template/Singleton.h"

namespace ms
{
	// The game data behind the name codes of an analyzed text
	// (textformat::Resolver). A lookup the data does not answer returns an empty
	// string, which the analyzer drops the code for.
	class TextResolver : public textformat::Resolver, public Singleton<TextResolver>
	{
	public:
		std::string item_name(int32_t itemid) const override;
		int32_t item_count(int32_t itemid) const override;
		Texture item_icon(int32_t itemid) const override;
		Texture skill_icon(int32_t skillid) const override;
		Texture ui_image(const std::string& path) const override;
		std::string mob_name(int32_t mobid) const override;
		std::string npc_name(int32_t npcid) const override;
		std::string map_name(int32_t mapid) const override;
		std::string skill_name(int32_t skillid) const override;
		std::string quest_name(int32_t questid) const override;
		std::string player_name() const override;
	};
}
