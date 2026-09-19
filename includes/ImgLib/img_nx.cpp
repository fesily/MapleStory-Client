//////////////////////////////////////////////////////////////////////////////////
//	Loose .img data backend: the .nx side ("PKG4" packages)						//
//																				//
//	Serves the records of a NoLifeNx package to the img_prop tree, so a category	//
//	can come from a .nx file while the loose .img folder still takes over the	//
//	entries it holds. Only the container is read here: the tree, the source		//
//	selection and the merging live in img_fs.cpp.								//
//																				//
//	Format reference: includes/NoLifeNx/nlnx/file_impl.hpp and node_impl.hpp	//
//	(the same fields Tool/Nx2Img.py::NxFile reads for the converter).			//
//////////////////////////////////////////////////////////////////////////////////
#include "../../MapleStory.h"

#ifdef USE_IMG

#include "img_impl.hpp"

#include <lz4.h>

#include <Windows.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
	// Fixed part of a package; the tables it points at follow it
#pragma pack(push, 1)
	struct nx_header
	{
		uint32_t magic;
		uint32_t node_count;
		uint64_t node_offset;
		uint32_t string_count;
		uint64_t string_offset;
		uint32_t bitmap_count;
		uint64_t bitmap_offset;
		uint32_t audio_count;
		uint64_t audio_offset;
	};

	// One record of the node table: name index, first child, child count, data type
	// and the value (integer, real, string index, vector, bitmap or audio entry)
	struct nx_record
	{
		uint32_t name;
		uint32_t children;
		uint16_t num;
		uint16_t type;
		uint64_t payload;
	};
#pragma pack(pop)

	static_assert(sizeof(nx_header) == 52, "PKG4 header layout");
	static_assert(sizeof(nx_record) == 20, "PKG4 node record layout");

	// A package that stays mapped for the lifetime of the process: the strings and
	// the payloads handed out through nl::node point straight into it
	struct nx_mapping
	{
		HANDLE file = INVALID_HANDLE_VALUE;
		HANDLE map = nullptr;
		uint8_t const* base = nullptr;
		size_t size = 0;

		// Shared between the split roots (Map/Map001/Map002 read one package) and
		// used by the logs
		std::string path;

		nx_header const* header = nullptr;
		nx_record const* nodes = nullptr;
		uint64_t const* strings = nullptr;
		uint64_t const* bitmaps = nullptr;
		uint64_t const* audios = nullptr;
	};

	// Program lifetime state, never destroyed: see img_immortal
	std::vector<std::unique_ptr<nx_mapping>>& g_mappings = nl::img_immortal<std::vector<std::unique_ptr<nx_mapping>>>();

	std::wstring to_wide(std::string const& path)
	{
		if (path.empty())
			return std::wstring();

		int length = ::MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);

		if (length <= 0)
			return std::wstring();

		std::wstring wide(static_cast<size_t>(length - 1), L'\0');
		::MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wide[0], length);

		return wide;
	}

	void close(nx_mapping& mapping)
	{
		if (mapping.base)
			::UnmapViewOfFile(mapping.base);

		if (mapping.map)
			::CloseHandle(mapping.map);

		if (mapping.file != INVALID_HANDLE_VALUE)
			::CloseHandle(mapping.file);

		mapping.base = nullptr;
		mapping.map = nullptr;
		mapping.file = INVALID_HANDLE_VALUE;
	}

	// Every table has to lie inside the mapping; a broken package must not be walked
	bool tables_fit(nx_mapping const& mapping)
	{
		nx_header const& header = *mapping.header;

		auto fits = [&mapping](uint64_t offset, uint64_t count, uint64_t width)
		{
			return offset <= mapping.size && count <= (mapping.size - offset) / width;
		};

		return fits(header.node_offset, header.node_count, sizeof(nx_record))
			&& fits(header.string_offset, header.string_count, sizeof(uint64_t))
			&& fits(header.bitmap_offset, header.bitmap_count, sizeof(uint64_t))
			&& fits(header.audio_offset, header.audio_count, sizeof(uint64_t));
	}
}

namespace nl
{
	int32_t img_nx_open(std::string const& path, std::string& error)
	{
		// One mapping per file: Map.nx serves Map, Map001, Map002 and Map2 at once
		for (size_t i = 0; i < g_mappings.size(); i++)
			if (g_mappings[i]->path == path)
				return static_cast<int32_t>(i);

		std::unique_ptr<nx_mapping> mapping(new nx_mapping());
		mapping->path = path;

		std::wstring wide = to_wide(path);

		if (wide.empty())
		{
			error = "not a usable path: " + path;
			return -1;
		}

		mapping->file = ::CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);

		if (mapping->file == INVALID_HANDLE_VALUE)
		{
			error = "cannot open " + path;
			return -1;
		}

		LARGE_INTEGER file_size = {};

		if (!::GetFileSizeEx(mapping->file, &file_size) || file_size.QuadPart < static_cast<LONGLONG>(sizeof(nx_header)))
		{
			close(*mapping);
			error = path + " is too small for an nx file";
			return -1;
		}

		mapping->size = static_cast<size_t>(file_size.QuadPart);
		mapping->map = ::CreateFileMappingW(mapping->file, nullptr, PAGE_READONLY, 0, 0, nullptr);

		if (!mapping->map)
		{
			close(*mapping);
			error = "cannot map " + path;
			return -1;
		}

		mapping->base = static_cast<uint8_t const*>(::MapViewOfFile(mapping->map, FILE_MAP_READ, 0, 0, 0));

		if (!mapping->base)
		{
			close(*mapping);
			error = "cannot map a view of " + path;
			return -1;
		}

		mapping->header = reinterpret_cast<nx_header const*>(mapping->base);

		if (mapping->header->magic != 0x34474B50)
		{
			close(*mapping);
			error = path + " is not a PKG4 nx file";
			return -1;
		}

		if (!tables_fit(*mapping))
		{
			close(*mapping);
			error = path + " has tables outside the file";
			return -1;
		}

		nx_header const& header = *mapping->header;
		mapping->nodes = reinterpret_cast<nx_record const*>(mapping->base + header.node_offset);
		mapping->strings = reinterpret_cast<uint64_t const*>(mapping->base + header.string_offset);
		mapping->bitmaps = reinterpret_cast<uint64_t const*>(mapping->base + header.bitmap_offset);
		mapping->audios = reinterpret_cast<uint64_t const*>(mapping->base + header.audio_offset);

		g_mappings.push_back(std::move(mapping));

		return static_cast<int32_t>(g_mappings.size() - 1);
	}

	std::string img_nx_string(int32_t file, uint32_t index)
	{
		if (file < 0 || static_cast<size_t>(file) >= g_mappings.size())
			return std::string();

		nx_mapping const& mapping = *g_mappings[file];

		if (index >= mapping.header->string_count)
			return std::string();

		uint64_t offset = mapping.strings[index];

		if (offset + 2 > mapping.size)
			return std::string();

		uint16_t length = 0;
		std::memcpy(&length, mapping.base + offset, sizeof(length));

		if (offset + 2 + length > mapping.size)
			return std::string();

		return std::string(reinterpret_cast<char const*>(mapping.base + offset + 2), length);
	}

	bool img_nx_record(int32_t file, uint32_t index, img_nx_node& out)
	{
		if (file < 0 || static_cast<size_t>(file) >= g_mappings.size())
			return false;

		nx_mapping const& mapping = *g_mappings[file];

		if (index >= mapping.header->node_count)
			return false;

		nx_record const& record = mapping.nodes[index];

		out.name = img_nx_string(file, record.name);
		out.type = record.type;
		out.children = record.children;
		out.num = record.num;
		out.payload = record.payload;

		return true;
	}

	bool img_nx_find(int32_t file, uint32_t parent, std::string const& name, uint32_t& out)
	{
		img_nx_node node;

		if (!img_nx_record(file, parent, node))
			return false;

		// The children of a record are sorted by name, which is what NoLifeNx's
		// get_child binary search relies on as well
		uint32_t low = 0;
		uint32_t high = node.num;

		while (low < high)
		{
			uint32_t middle = low + (high - low) / 2;
			std::string candidate = img_nx_string(file, g_mappings[file]->nodes[node.children + middle].name);

			if (candidate == name)
			{
				out = node.children + middle;
				return true;
			}

			if (candidate < name)
				low = middle + 1;
			else
				high = middle;
		}

		return false;
	}

	bool img_nx_bitmap(int32_t file, uint32_t index, uint8_t const*& ptr, uint32_t& size)
	{
		if (file < 0 || static_cast<size_t>(file) >= g_mappings.size())
			return false;

		nx_mapping const& mapping = *g_mappings[file];

		if (index >= mapping.header->bitmap_count)
			return false;

		uint64_t offset = mapping.bitmaps[index];

		if (offset + 4 > mapping.size)
			return false;

		uint32_t stored = 0;
		std::memcpy(&stored, mapping.base + offset, sizeof(stored));

		if (offset + 4 + stored > mapping.size)
			return false;

		ptr = mapping.base + offset;
		size = 4 + stored;

		return true;
	}

	bool img_nx_audio(int32_t file, uint32_t index, uint32_t length, uint8_t const*& ptr, uint32_t& size)
	{
		if (file < 0 || static_cast<size_t>(file) >= g_mappings.size())
			return false;

		nx_mapping const& mapping = *g_mappings[file];

		if (index >= mapping.header->audio_count || length == 0)
			return false;

		uint64_t offset = mapping.audios[index];

		if (offset + length > mapping.size)
			return false;

		ptr = mapping.base + offset;
		size = length;

		return true;
	}

	std::unique_ptr<img_prop> img_nx_read(int32_t file, uint32_t index, std::string const& name)
	{
		img_nx_node record;

		if (!img_nx_record(file, index, record))
			return nullptr;

		std::unique_ptr<img_prop> prop(new img_prop());
		prop->name = name;
		prop->nx_file = file;
		prop->nx_index = index;
		prop->nx_kids = record.num;

		switch (record.type)
		{
		case 0:		// a container: the caller attaches the loose side and the path
			prop->kind = IMG_NX;
			break;
		case 1:
			prop->kind = IMG_INT;
			prop->ival = static_cast<int64_t>(record.payload);
			break;
		case 2:
			prop->kind = IMG_REAL;
			std::memcpy(&prop->rval, &record.payload, sizeof(prop->rval));
			break;
		case 3:
			prop->kind = IMG_STRING;
			prop->sval = img_nx_string(file, static_cast<uint32_t>(record.payload & 0xFFFFFFFF));
			break;
		case 4:
			prop->kind = IMG_VECTOR;
			std::memcpy(&prop->vx, &record.payload, sizeof(prop->vx));
			std::memcpy(&prop->vy, reinterpret_cast<uint8_t const*>(&record.payload) + 4, sizeof(prop->vy));
			break;
		case 5:
		{
			prop->kind = IMG_CANVAS;
			prop->codec = IMG_CODEC_LZ4;
			prop->width = static_cast<uint16_t>((record.payload >> 32) & 0xFFFF);
			prop->height = static_cast<uint16_t>((record.payload >> 48) & 0xFFFF);

			uint8_t const* ptr = nullptr;
			uint32_t size = 0;

			if (!img_nx_bitmap(file, static_cast<uint32_t>(record.payload & 0xFFFFFFFF), ptr, size))
				prop->failed = true;
			else
			{
				prop->blob_ptr = ptr;
				prop->blob_size = size;
			}

			break;
		}
		case 6:
		{
			prop->kind = IMG_AUDIO;

			uint8_t const* ptr = nullptr;
			uint32_t size = 0;

			if (!img_nx_audio(file, static_cast<uint32_t>(record.payload & 0xFFFFFFFF), static_cast<uint32_t>((record.payload >> 32) & 0xFFFFFFFF), ptr, size))
				prop->failed = true;
			else
			{
				prop->blob_ptr = ptr;
				prop->blob_size = size;
			}

			break;
		}
		default:
			img_log_error("Unknown nx node type " + std::to_string(record.type) + " in " + g_mappings[file]->path);
			prop->kind = IMG_NULL;
			break;
		}

		return prop;
	}
}

#endif // USE_IMG
