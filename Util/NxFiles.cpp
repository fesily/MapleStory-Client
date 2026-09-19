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
#include "NxFiles.h"
#include "../Configuration.h"

#ifdef USE_NX
// With USE_IMG the .nx backend is replaced by includes/ImgLib, which reads the
// same categories from the loose .img folder; nothing calls into NxFiles then,
// and nl::file (NoLifeNx) is not linked.
#ifndef USE_IMG
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlnx/file.hpp>
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>

namespace ms
{
	namespace NxFiles
	{
		namespace
		{
			// Root directory for the *.nx files, empty or "." means cwd
			std::string data_dir()
			{
				std::string dir = Setting<DataPath>::get().load();

				while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\'))
					dir.pop_back();

				return dir;
			}

			bool use_cwd(const std::string& dir)
			{
				return dir.empty() || dir == ".";
			}

			std::string join(const std::string& dir, const char* name)
			{
				if (use_cwd(dir))
					return name;

				return dir + '/' + name;
			}

			bool exists(const std::string& path)
			{
				return std::ifstream{ path }.is_open();
			}
		}

		Error init()
		{
			const std::string dir = data_dir();

			for (auto filename : filenames)
			{
				if (exists(join(dir, filename)) == false)
				{
					static std::string missing;
					missing = join(dir, filename);

					return Error(Error::Code::MISSING_FILE, missing.c_str());
				}
			}

			// Owns the mapped files so the nodes in nl::nx stay valid
			static std::vector<std::unique_ptr<nl::file>> files;

			auto add_file = [](const std::string& path)
			{
				if (exists(path) == false)
					return nl::node{};

				files.emplace_back(new nl::file(path));

				return nl::node{ *files.back() };
			};

			try
			{
				if (exists(join(dir, "Base.nx")))
				{
					std::pair<const char*, nl::node*> targets[] =
					{
						{ "Base.nx", &nl::nx::Base },
						{ "Character.nx", &nl::nx::Character },
						{ "Effect.nx", &nl::nx::Effect },
						{ "Etc.nx", &nl::nx::Etc },
						{ "Item.nx", &nl::nx::Item },
						{ "Map.nx", &nl::nx::Map },
						{ "Map001.nx", &nl::nx::Map001 },
						{ "Map002.nx", &nl::nx::Map002 },
						{ "Map2.nx", &nl::nx::Map2 },
						{ "Mob.nx", &nl::nx::Mob },
						{ "Mob001.nx", &nl::nx::Mob001 },
						{ "Mob002.nx", &nl::nx::Mob002 },
						{ "Mob2.nx", &nl::nx::Mob2 },
						{ "Morph.nx", &nl::nx::Morph },
						{ "Npc.nx", &nl::nx::Npc },
						{ "Quest.nx", &nl::nx::Quest },
						{ "Reactor.nx", &nl::nx::Reactor },
						{ "Skill.nx", &nl::nx::Skill },
						{ "Skill001.nx", &nl::nx::Skill001 },
						{ "Skill002.nx", &nl::nx::Skill002 },
						{ "Skill003.nx", &nl::nx::Skill003 },
						{ "Sound.nx", &nl::nx::Sound },
						{ "Sound001.nx", &nl::nx::Sound001 },
						{ "Sound002.nx", &nl::nx::Sound002 },
						{ "Sound2.nx", &nl::nx::Sound2 },
						{ "String.nx", &nl::nx::String },
						{ "TamingMob.nx", &nl::nx::TamingMob },
						{ "UI.nx", &nl::nx::UI },
					};

					for (auto& target : targets)
						*target.second = add_file(join(dir, target.first));
				}
				else if (exists(join(dir, "Data.nx")))
				{
					nl::nx::Base = add_file(join(dir, "Data.nx"));
					nl::nx::Character = nl::nx::Base["Character"];
					nl::nx::Effect = nl::nx::Base["Effect"];
					nl::nx::Etc = nl::nx::Base["Etc"];
					nl::nx::Item = nl::nx::Base["Item"];
					nl::nx::Map = nl::nx::Base["Map"];
					nl::nx::Map001 = nl::nx::Base["Map001"];
					nl::nx::Map002 = nl::nx::Base["Map002"];
					nl::nx::Map2 = nl::nx::Base["Map2"];
					nl::nx::Mob = nl::nx::Base["Mob"];
					nl::nx::Mob001 = nl::nx::Base["Mob001"];
					nl::nx::Mob002 = nl::nx::Base["Mob002"];
					nl::nx::Mob2 = nl::nx::Base["Mob2"];
					nl::nx::Morph = nl::nx::Base["Morph"];
					nl::nx::Npc = nl::nx::Base["Npc"];
					nl::nx::Quest = nl::nx::Base["Quest"];
					nl::nx::Reactor = nl::nx::Base["Reactor"];
					nl::nx::Skill = nl::nx::Base["Skill"];
					nl::nx::Skill001 = nl::nx::Base["Skill001"];
					nl::nx::Skill002 = nl::nx::Base["Skill002"];
					nl::nx::Skill003 = nl::nx::Base["Skill003"];
					nl::nx::Sound = nl::nx::Base["Sound"];
					nl::nx::Sound001 = nl::nx::Base["Sound001"];
					nl::nx::Sound002 = nl::nx::Base["Sound002"];
					nl::nx::Sound2 = nl::nx::Base["Sound2"];
					nl::nx::String = nl::nx::Base["String"];
					nl::nx::TamingMob = nl::nx::Base["TamingMob"];
					nl::nx::UI = nl::nx::Base["UI"];
				}
				else
				{
					throw std::runtime_error("Failed to locate nx files.");
				}
			}
			catch (const std::exception& ex)
			{
				static const std::string message = ex.what();

				return Error(Error::Code::NLNX, message.c_str());
			}

			constexpr const char* POSTCHAOS_BITMAP = "Login.img/WorldSelect/BtChannel/layer:bg";

			if (nl::nx::UI.resolve(POSTCHAOS_BITMAP).data_type() != nl::node::type::bitmap)
				return Error::Code::WRONG_UI_FILE;

			return Error::Code::NONE;
		}
	}
}
#endif

#endif // USE_IMG
