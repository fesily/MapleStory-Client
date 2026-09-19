//////////////////////////////////////////////////////////////////////////////////
//	Loose .img data folder: file system layer									//
//																				//
//	Resolves																	//
//		<data>/<Category>/<directory...>/<Name>.img								//
//	into the node tree the client walks. Category directories replace the .nx	//
//	files: every WZ image becomes a file, every WZ directory a directory, and	//
//	multi-part WZ files are merged into a single category directory (Map001 and	//
//	Map002 both read from <data>/Map).											//
//																				//
//	Directories are listed and images parsed on first use and kept for the		//
//	lifetime of the process, so the nl::node pointers handed out stay valid.	//
//	Directory entries are shared between aliases (Map/Map001/Map002), so a node	//
//	returned twice for the same path is identical.								//
//////////////////////////////////////////////////////////////////////////////////
#include "../../MapleStory.h"

#ifdef USE_IMG
#include "img_impl.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include <Windows.h>

namespace
{
	// The roots the client knows, mirroring nl::nx::* in NoLifeNx
	char const* const ROOT_NAMES[] =
	{
		"Base", "Character", "Effect", "Etc", "Item", "Map", "Map001", "Map002", "Map2",
		"Mob", "Mob001", "Mob002", "Mob2", "Morph", "Npc", "Quest", "Reactor", "Skill",
		"Skill001", "Skill002", "Skill003", "Sound", "Sound001", "Sound002", "Sound2",
		"String", "TamingMob", "UI"
	};

	// Program lifetime state of the backend, never destroyed: see img_immortal
	std::string& g_data_dir = nl::img_immortal<std::string>();

	// The parsed tree is never destroyed (see img_immortal): nodes handed out to
	// the client stay valid for as long as any of its singletons hold them.
	std::vector<std::unique_ptr<nl::img_prop>>& g_root_storage = nl::img_immortal<std::vector<std::unique_ptr<nl::img_prop>>>();
	std::unordered_map<std::string, nl::img_prop*>& g_roots = nl::img_immortal<std::unordered_map<std::string, nl::img_prop*>>();
	std::unordered_map<std::string, std::unique_ptr<nl::img_prop>>& g_nodes = nl::img_immortal<std::unordered_map<std::string, std::unique_ptr<nl::img_prop>>>();

	std::string normalize(std::string path, bool directory)
	{
		std::replace(path.begin(), path.end(), '\\', '/');

		while (directory && !path.empty() && path.back() == '/')
			path.pop_back();

		if (directory)
			path.push_back('/');

		return path;
	}

	std::string first_segment(std::string const& path)
	{
		size_t end = path.find('/');
		return end == std::string::npos ? path : path.substr(0, end);
	}

	std::string segment_after_first(std::string const& path)
	{
		size_t end = path.find('/');
		return end == std::string::npos ? std::string() : path.substr(end + 1);
	}

	std::string last_segment(std::string path)
	{
		while (!path.empty() && (path.back() == '/'))
			path.pop_back();

		size_t start = path.find_last_of('/');
		return start == std::string::npos ? path : path.substr(start + 1);
	}

	// "Map001" -> "Map", "Sound002" -> "Sound": the directory a root reads from
	std::string base_category(std::string name)
	{
		while (!name.empty() && name.back() >= '0' && name.back() <= '9')
			name.pop_back();

		return name;
	}

	bool is_directory(std::string const& abs)
	{
		DWORD attributes = GetFileAttributesA(abs.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	bool is_file(std::string const& abs)
	{
		DWORD attributes = GetFileAttributesA(abs.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	nl::img_prop* create_node(std::string const& path, bool directory)
	{
		std::unique_ptr<nl::img_prop> prop(new nl::img_prop());
		prop->name = last_segment(path);
		prop->kind = directory ? nl::IMG_DIR : nl::IMG_FILE;
		prop->path = directory ? normalize(path, true) : path;
		prop->root = nl::img_root_for_path(path);
		nl::img_prop* raw = prop.get();
		g_nodes.emplace(path, std::move(prop));
		return raw;
	}

	void list_directory(nl::img_prop* dir)
	{
		dir->loaded = true;

		std::string pattern = g_data_dir + dir->path + "*";
		WIN32_FIND_DATAA entry;
		HANDLE find = FindFirstFileA(pattern.c_str(), &entry);

		if (find == INVALID_HANDLE_VALUE)
			return;

		std::vector<std::string> names;

		do
		{
			std::string name = entry.cFileName;

			if (name == "." || name == "..")
				continue;

			bool directory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

			if (!directory && (name.size() < 4 || name.compare(name.size() - 4, 4, ".img") != 0))
				continue;

			names.push_back(std::move(name));
		}
		while (FindNextFileA(find, &entry));

		FindClose(find);

		std::sort(names.begin(), names.end());

		for (auto& name : names)
		{
			std::string path = dir->path + name;
			nl::img_prop* child = nl::img_node_for_path(path, is_directory(g_data_dir + path));

			if (child)
				dir->kids.push_back(child);
		}

		// Directory entries are shared between aliases; index and parent are only
		// used to walk a listing, and directory listings are never iterated.
		for (uint32_t i = 0; i < dir->kids.size(); i++)
		{
			dir->kids[i]->parent = dir;
			dir->kids[i]->index = i;
		}
	}

	void parse_image(nl::img_prop* file)
	{
		file->loaded = true;

		std::vector<std::unique_ptr<nl::img_prop>> props;

		if (!nl::img_parse(g_data_dir + file->path, props))
		{
			file->failed = true;
			nl::img_log_error("Could not parse image file: " + file->path);
			return;
		}

		nl::img_adopt_children(file, std::move(props));
	}

	bool has_img_suffix(std::string const& name)
	{
		return name.size() >= 4 && name.compare(name.size() - 4, 4, ".img") == 0;
	}

	// The loose folder takes over the entries of a package it holds. A directory
	// merges with the file (the file keeps serving what the directory does not
	// hold), a .img file replaces the subtree outright. Nothing can be overridden
	// below a node whose directory does not exist, so those are never probed.
	nl::img_prop* take_over(nl::img_prop* dir, std::string const& name)
	{
		for (nl::img_prop* kid : dir->kids)
		{
			if (kid->name == name)
				return kid;

			if (kid->name == name + ".img")
				return kid;
		}

		if (dir->fs_absent)
			return nullptr;

		std::string stem = dir->path + name;
		bool directory = is_directory(g_data_dir + stem);

		if (directory)
			return nl::img_node_for_path(stem, true);

		if (is_file(g_data_dir + stem))
			return nl::img_node_for_path(stem, false);

		if (!has_img_suffix(name) && is_file(g_data_dir + stem + ".img"))
			return nl::img_node_for_path(stem + ".img", false);

		return nullptr;
	}

	// Merges the entries of the package behind a container into its children: what
	// the loose folder holds stays (it takes over the record of the same name),
	// everything else is read from the file
	void expand_nx(nl::img_prop* dir)
	{
		nl::img_nx_node node;

		if (!nl::img_nx_record(dir->nx_file, dir->nx_index, node))
		{
			dir->failed = true;
			nl::img_log_error("Could not read the nx record behind " + dir->name);
			return;
		}

		for (uint32_t i = 0; i < node.num; i++)
		{
			uint32_t index = node.children + i;
			nl::img_nx_node record;

			if (!nl::img_nx_record(dir->nx_file, index, record))
				continue;

			nl::img_prop* loose = take_over(dir, record.name);

			if (loose)
			{
				// A directory keeps the record behind it, so the entries it does not
				// hold are still served from the file
				if (loose->kind == nl::IMG_DIR && loose->nx_file < 0)
				{
					loose->nx_file = dir->nx_file;
					loose->nx_index = index;
					loose->nx_kids = record.num;
				}

				LOG(LOG_INFO, "[ImgLib] takes over: " << loose->path << (loose->kind == nl::IMG_FILE ? " (image)" : ""));
				continue;
			}

			std::unique_ptr<nl::img_prop> kid = nl::img_nx_read(dir->nx_file, index, record.name);

			if (!kid)
				continue;

			// A node the package gave children to is a container here as well: a
			// canvas carries origin and delay, and the loose folder may hold that
			// path just like any other
			if (kid->kind == nl::IMG_NX || kid->nx_kids > 0)
			{
				std::string stem = dir->path + record.name;
				kid->path = stem + "/";
				kid->root = nl::img_root_for_path(stem);
				kid->fs_absent = !is_directory(g_data_dir + stem);
			}

			dir->kids.push_back(kid.get());
			dir->owned.emplace_back(std::move(kid));
		}

		std::sort(dir->kids.begin(), dir->kids.end(), [](nl::img_prop const* left, nl::img_prop const* right)
		{
			return left->name < right->name;
		});

		for (uint32_t i = 0; i < dir->kids.size(); i++)
		{
			dir->kids[i]->parent = dir;
			dir->kids[i]->index = i;
		}
	}
}

namespace nl
{
	img_prop const* img_root_for_path(std::string const& path)
	{
		std::string category = first_segment(path);
		std::string rest = segment_after_first(path);

		// A folder holding both the packages and the loose .img folder they were
		// converted to keeps the latter in "data", which reads as a plain prefix
		// on every path below it
		if (category == "data")
		{
			category = first_segment(rest);
			rest = segment_after_first(rest);
		}

		if (category == "Map")
		{
			std::string sub = first_segment(rest);

			if (sub == "Back")
				return g_roots["Map001"];

			if (sub == "Map" || sub == "Effect.img")
				return g_roots["Map002"];

			return g_roots["Map"];
		}

		auto found = g_roots.find(category);
		return found == g_roots.end() ? nullptr : found->second;
	}

	img_prop* img_node_for_path(std::string const& path, bool directory)
	{
		auto found = g_nodes.find(path);

		if (found != g_nodes.end())
			return found->second.get();

		if (directory)
			return is_directory(g_data_dir + path) ? create_node(path, true) : nullptr;

		return is_file(g_data_dir + path) ? create_node(path, false) : nullptr;
	}

	void img_ensure_loaded(img_prop const* prop)
	{
		img_prop* mutable_prop = const_cast<img_prop*>(prop);

		if (mutable_prop->loaded || mutable_prop->failed)
			return;

		mutable_prop->loaded = true;

		if (mutable_prop->kind == IMG_DIR)
			list_directory(mutable_prop);
		else if (mutable_prop->kind == IMG_FILE)
			parse_image(mutable_prop);

		// Whatever the loose folder provided, the package behind the node merges
		// the entries it holds into it (a .img file replaced the subtree outright,
		// so it has no file side left to read)
		if (mutable_prop->nx_file >= 0 && mutable_prop->nx_kids > 0 && mutable_prop->kind != IMG_FILE)
			expand_nx(mutable_prop);
	}

	img_prop const* img_lookup(img_prop const* prop, std::string const& name)
	{
		if (!prop)
			return nullptr;

		if (!img_has_children(prop))
			return nullptr;

		img_ensure_loaded(prop);

		for (img_prop* kid : prop->kids)
		{
			if (kid->name == name)
				return kid;
		}

		// Tolerate lookups without the .img suffix (the loose folder and the
		// packages name their children the same way, so both tolerate it)
		if ((prop->kind == IMG_DIR || prop->kind == IMG_NX) && !has_img_suffix(name))
		{
			for (img_prop* kid : prop->kids)
			{
				if (kid->name == name + ".img")
					return kid;
			}
		}

		return nullptr;
	}

	void img_set_data_dir(std::string dir)
	{
		g_data_dir = normalize(std::move(dir), true);
	}

	std::string const& img_data_dir()
	{
		return g_data_dir;
	}

	bool img_has_children(img_prop const* prop)
	{
		return prop->kind == IMG_DIR || prop->kind == IMG_NX || prop->kind == IMG_FILE
			|| prop->kind == IMG_SUB || prop->kind == IMG_CANVAS || prop->nx_kids > 0;
	}

	bool img_root_has_source(std::string const& name)
	{
		return g_roots.count(name) != 0;
	}

	node img_root(std::string const& name)
	{
		auto found = g_roots.find(name);
		return found == g_roots.end() ? img_access::make(nullptr, nullptr) : img_access::make(found->second, nullptr);
	}

	void img_load_roots()
	{
		if (g_data_dir.empty() || !is_directory(g_data_dir))
			throw std::runtime_error("Failed to locate the data folder: " + g_data_dir);

		g_root_storage.reserve(sizeof(ROOT_NAMES) / sizeof(ROOT_NAMES[0]));

		for (char const* name : ROOT_NAMES)
		{
			std::string category = base_category(name);

			// The loose side of a root: the categories directly inside the data
			// folder, or in its "data" folder when the packages sit next to it
			std::string stem = category + "/";
			bool has_directory = is_directory(g_data_dir + stem);

			if (!has_directory && is_directory(g_data_dir + "data/" + stem))
			{
				stem = "data/" + stem;
				has_directory = true;
			}

			// The .nx side of a root: the file named after it, the package of its
			// category (a pre-split set keeps Map001/Map002/Map2 in Map.nx) or the
			// single package that holds every category (Data.nx)
			int32_t nx_file = -1;
			uint32_t nx_index = 0;
			std::string nx_path;

			std::string candidates[] = { std::string(name) + ".nx", category + ".nx" };

			for (std::string const& candidate : candidates)
			{
				if (!is_file(g_data_dir + candidate))
					continue;

				std::string error;
				nx_file = nl::img_nx_open(g_data_dir + candidate, error);

				if (nx_file < 0)
					throw std::runtime_error(error);

				nx_path = candidate;
				break;
			}

			if (nx_file < 0 && is_file(g_data_dir + "Data.nx"))
			{
				std::string error;
				int32_t data_file = nl::img_nx_open(g_data_dir + "Data.nx", error);

				if (data_file < 0)
					throw std::runtime_error(error);

				uint32_t index = 0;

				if (nl::img_nx_find(data_file, 0, name, index))
				{
					nx_file = data_file;
					nx_index = index;
					nx_path = "Data.nx";
				}
			}

			if (!has_directory && nx_file < 0)
			{
				LOG(LOG_WARN, "[ImgLib] no data for " << name << ": neither " << stem << " nor " << name << ".nx exists");
				continue;
			}

			// A root carries both sides at once: the entries of the package, with
			// every one of them the loose folder holds taken over
			std::unique_ptr<img_prop> prop(new img_prop());
			prop->name = name;
			prop->kind = has_directory ? IMG_DIR : IMG_NX;
			prop->path = stem;
			prop->root = prop.get();
			prop->nx_file = nx_file;
			prop->nx_index = nx_index;
			prop->fs_absent = !has_directory;

			if (nx_file >= 0)
			{
				nl::img_nx_node record;

				if (nl::img_nx_record(nx_file, nx_index, record))
					prop->nx_kids = record.num;
			}

			LOG(LOG_INFO, "[ImgLib] " << name << ": " << (has_directory ? ("img(" + stem + ")") : "") << (has_directory && nx_file >= 0 ? " + " : "") << (nx_file >= 0 ? ("nx(" + nx_path + ")") : ""));

			g_roots[name] = prop.get();
			g_root_storage.emplace_back(std::move(prop));
		}
	}

	void img_log_error(std::string const& message)
	{
		LOG(LOG_ERROR, message);
		OutputDebugStringA((message + "\n").c_str());
	}
}

#endif // USE_IMG
