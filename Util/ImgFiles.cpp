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
#include "ImgFiles.h"

#ifdef USE_IMG

#include "../includes/ImgLib/img_impl.hpp"

#include <nlnx/nx.hpp>

#include "../Configuration.h"

#include <Windows.h>

#include <iostream>

namespace ms
{
	namespace ImgFiles
	{
		// Categories the client reads from. A missing one is reported the same way a
		// missing .nx file was; each of them may come from the loose .img folder, a
		// .nx package or a Data.nx package, which the backend logs per root.
		constexpr char const* REQUIRED[] =
		{
			"UI", "String", "Map", "Character", "Mob", "Npc", "Item",
			"Skill", "Effect", "Etc", "Sound", "Reactor"
		};

		// The image layout every client with a post-Chaos UI ships
		constexpr char const* POSTCHAOS_BITMAP = "Login.img/WorldSelect/BtChannel/layer:bg";
		// Older or locally rebuilt UIs use this node instead
		constexpr char const* LEGACY_WORLDSELECT = "Login.img/WorldSelect/BtWorld";

		bool is_directory(std::string const& path)
		{
			DWORD attributes = GetFileAttributesA(path.c_str());

			return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		}

		// Every data set ships UI, so its presence tells a data folder from any directory
		bool has_categories(std::string const& dir)
		{
			return is_directory(dir + "/UI");
		}

		std::string find_data_dir()
		{
			// DataPath from the settings file wins, so the client can be started
			// from any working directory. Its default (".") means the working
			// directory itself, which only counts when the categories are in it.
			std::string configured = Setting<DataPath>::get().load();

			if (!configured.empty() && configured != ".")
			{
				if (!is_directory(configured))
				{
					LOG(LOG_ERROR, "DataPath is not a directory: " << configured);
					return std::string();
				}

				return configured;
			}

			std::string names[] = { ".", "data", "Data" };

			for (std::string const& name : names)
				if (has_categories(name))
					return name;

			return std::string();
		}

		Error init()
		{
			std::string directory = find_data_dir();

			if (directory.empty())
				return Error(Error::Code::MISSING_FILE, "data");

			nl::img_set_data_dir(directory);
			nl::img_set_trace_missing(Setting<TraceMissing>::get().load());
			LOG(LOG_INFO, "[ImgLib] data folder: " << nl::img_data_dir());

			try
			{
				nl::nx::load_all();
			}
			catch (const std::exception& ex)
			{
				return Error(Error::Code::IMG, ex.what());
			}

			// Every category the client reads has to have a source; the backend logs
			// which one each root was given
			for (char const* category : REQUIRED)
				if (!nl::img_root_has_source(category))
					return Error(Error::Code::MISSING_FILE, category);

			if (nl::nx::UI.resolve(POSTCHAOS_BITMAP).data_type() != nl::node::type::bitmap)
			{
				if (!nl::nx::UI.resolve(LEGACY_WORLDSELECT))
					return Error::Code::WRONG_UI_FILE;

				LOG(LOG_WARN, "UI/Login.img has no WorldSelect/BtChannel: the UI is from a different client version");
			}

			return Error::Code::NONE;
		}
	}
}

#endif
