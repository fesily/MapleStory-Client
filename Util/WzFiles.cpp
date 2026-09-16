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
#include "WzFiles.h"
#include "../Configuration.h"

#ifndef USE_NX
#include <fstream>
#include <string>

namespace ms
{
	namespace WzFiles
	{
		Error init()
		{
			std::string dir = Setting<DataPath>::get().load();

			while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\'))
				dir.pop_back();

			bool use_cwd = dir.empty() || dir == ".";

			for (auto filename : filenames)
			{
				std::string path = use_cwd ? filename : dir + '/' + filename;

				if (std::ifstream{ path }.good() == false)
				{
					static std::string missing;
					missing = path;

					return Error(Error::Code::MISSING_FILE, missing.c_str());
				}
			}

			try
			{
				//nl::nx::load_all();
			}
			catch (const std::exception& ex)
			{
				static const std::string message = ex.what();

				return Error(Error::Code::WZ, message.c_str());
			}

			//constexpr const char* POSTCHAOS_BITMAP = "Login.img/WorldSelect/BtChannel/layer:bg";
			//
			//if (nl::nx::ui.resolve(POSTCHAOS_BITMAP).data_type() != nl::node::type::bitmap)
			//	return Error::Code::WRONG_UI_FILE;

			return Error::Code::NONE;
		}
	}
}
#endif