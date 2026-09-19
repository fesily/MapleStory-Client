//////////////////////////////////////////////////////////////////////////////////
//	Loose .img data folder backend ("USE_IMG")										//
//																				//
//	Reads MapleStory standalone image files, i.e. the directory layout used by		//
//	private server clients and HaRepacker's "IMG filesystem":					//
//																				//
//		<data>/<Category>/<directory...>/<Name>.img								//
//																				//
//	Category directories take the place of the .nx files the client was			//
//	originally written for (NoLifeNx). Multi-part files are merged into a		//
//	single category directory: Map/Map001/Map002/Map2 all live in <data>/Map.	//
//																				//
//	Format reference: MapleLib WzImgDeserializer / WzImageProperty /			//
//	WzPngProperty (Harepacker-resurrected) and WzComparerR2 Wz_Png /			//
//	ChunkedEncryptedInputStream.												//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../MapleStory.h"

#include <nlnx/node.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nl
{
	struct img_prop;

	// Context handed to nl::node (node keeps _file_data const* m_file)
	struct _file_data
	{
		img_prop const* root = nullptr;
	};
}

// Payload handed to nl::node (node keeps data const* m_data)
struct nl::node::data
{
	nl::img_prop const* prop = nullptr;
};

namespace nl
{
	// Kinds of img_prop; IMG_DIR/IMG_FILE are containers, everything else is data
	enum img_kind : uint8_t
	{
		IMG_NULL = 0,
		IMG_INT,
		IMG_REAL,
		IMG_STRING,
		IMG_VECTOR,
		IMG_CANVAS,
		IMG_AUDIO,
		IMG_SUB,
		IMG_DIR,
		IMG_FILE,
		// A container whose entries live in a mapped .nx file (img_nx.cpp); the
		// loose .img folder still takes over every entry of it found on disk
		IMG_NX
	};

	// How the canvas payload of a node is stored
	enum img_codec : uint8_t
	{
		IMG_CODEC_NONE = 0,
		IMG_CODEC_ZLIB,		// loose .img canvas: AES chunked, zlib, format per canvas
		IMG_CODEC_LZ4		// .nx canvas: 4 byte header + LZ4 block, always BGRA8888
	};

	// A single node of an image, or a container entry of the data directory tree.
	// Properties are heap allocated once and never moved, so pointers handed out
	// through nl::node stay valid for the lifetime of the process.
	struct img_prop
	{
		std::string name;
		img_kind kind = IMG_NULL;

		int64_t ival = 0;
		double rval = 0.0;
		std::string sval;
		int32_t vx = 0;
		int32_t vy = 0;

		// canvas: compressed payload, audio: 82 byte header + sound data
		std::vector<uint8_t> payload;
		int32_t fmt = 0;
		uint16_t width = 0;
		uint16_t height = 0;

		// owned: property tree storage for images; kids: child list used for
		// lookups and iteration (directory entries point at shared nodes)
		std::vector<std::unique_ptr<img_prop>> owned;
		std::vector<img_prop*> kids;
		img_prop const* parent = nullptr;
		uint32_t index = 0;

		// IMG_DIR / IMG_NX only: the path of the directory on disk (trailing '/'),
		// which is also the path the loose folder is probed at
		std::string path;
		img_prop const* root = nullptr;
		bool loaded = false;

		// The record this node stands for in a mapped .nx file; -1 when the node
		// has no .nx behind it. A container carries both sides at once: what the
		// loose folder holds takes over the entries the file provides.
		int32_t nx_file = -1;
		uint32_t nx_index = 0;
		uint16_t nx_kids = 0;

		// Payload living in the mapping (canvas block, audio blob). The mapping is
		// never released, so the pointer stays valid without a copy being made.
		uint8_t const* blob_ptr = nullptr;
		uint32_t blob_size = 0;

		// How the payload is encoded
		img_codec codec = IMG_CODEC_NONE;

		// No directory of this name on disk: nothing at or below this node can be
		// taken over, so the loose folder is never consulted for it again
		bool fs_absent = false;

		mutable bool failed = false;			// parse/decode failed once, don't retry
		mutable std::vector<uint8_t> decoded;	// canvas pixels, BGRA8888
		mutable size_t uid = 0;					// stable identity of this canvas

		// Storage the nl::node handed out for this property points at
		node::data nd;
		_file_data fd;
	};

	// Constructor access for the backend (nl::node's data ctor is private)
	struct img_access
	{
		static node make(img_prop const* p, img_prop const* root);
		// The path a node sits at ("Map/Obj/login.img/WorldSelect/default/0"), for
		// diagnostics; empty for a node that is not in the tree
		static std::string path(node const& n);
	};

	// ---- used by Util/ImgFiles -------------------------------------------------
	void img_set_data_dir(std::string dir);
	std::string const& img_data_dir();
	// Creates the root nodes; throws std::runtime_error if the data dir is unusable.
	void img_load_roots();
	node img_root(std::string const& name);
	// Whether a root found a source: the loose .img folder or a .nx package
	bool img_root_has_source(std::string const& name);

	// ---- internals ------------------------------------------------------------
	// Whether a node can hold children: the containers of the loose folder, and
	// any node a package gave children to (canvases carry origin/delay as well)
	bool img_has_children(img_prop const* prop);
	// The path of a property in the tree, see img_access::path
	std::string img_prop_path(img_prop const* prop);
	// Logs every lookup the data does not answer (TraceMissing), which is how a
	// screen that comes up empty shows what it asked for
	void img_set_trace_missing(bool on);
	img_prop const* img_lookup(img_prop const* p, std::string const& name);
	void img_ensure_loaded(img_prop const* p);
	std::vector<uint8_t> const& img_keystream(size_t length);
	std::vector<uint8_t> img_inflate(std::vector<uint8_t> const& payload, size_t expected, size_t offset);
	std::vector<uint8_t> img_canvas_pixels(img_prop const& p);
	void img_log_error(std::string const& message);

	// img_parse.cpp
	bool img_parse(std::string const& path, std::vector<std::unique_ptr<img_prop>>& out);
	void img_adopt_children(img_prop* parent, std::vector<std::unique_ptr<img_prop>> kids);

	// img_nx.cpp
	// Opens a PKG4 package and keeps it mapped for the lifetime of the process;
	// returns the mapping slot, or -1 with the reason in error
	int32_t img_nx_open(std::string const& path, std::string& error);
	// One record of the node table: name, data type, first child, child count and
	// the value (integer, real, string index, vector, bitmap or audio entry)
	struct img_nx_node
	{
		std::string name;
		uint16_t type = 0;
		uint32_t children = 0;
		uint16_t num = 0;
		uint64_t payload = 0;
	};
	bool img_nx_record(int32_t file, uint32_t index, img_nx_node& out);
	// Looks a child up by name; the records keep their children sorted
	bool img_nx_find(int32_t file, uint32_t parent, std::string const& name, uint32_t& out);
	std::string img_nx_string(int32_t file, uint32_t index);
	// Pointers into the mapping: canvas block (4 byte header + LZ4) and audio blob
	bool img_nx_bitmap(int32_t file, uint32_t index, uint8_t const*& ptr, uint32_t& size);
	bool img_nx_audio(int32_t file, uint32_t index, uint32_t length, uint8_t const*& ptr, uint32_t& size);
	// Turns one record into a node of the tree: values are read, payloads are
	// referenced. A container record comes back as IMG_NX without a path, which
	// the file system side (img_fs.cpp) attaches together with the loose folder.
	std::unique_ptr<img_prop> img_nx_read(int32_t file, uint32_t index, std::string const& name);

	// ---- data layer lifetime ----------------------------------------------------
	// The client's singletons are namespace scope objects registered with atexit
	// from their own translation units, so they are torn down in an order this
	// backend cannot predict: Stage still releases its textures after the caches
	// below were destroyed. The backend keeps its state on the heap and
	// deliberately never frees it - the data layer has to outlive every reader.
	template <typename T>
	T& img_immortal()
	{
		static T* const cell = new T;

		return *cell;
	}

	// ---- decoded canvas pixels -------------------------------------------------
	// Whoever draws a bitmap has to keep its pixels alive for as long as it uses
	// them (a Texture pins them for its lifetime): the decode cache prefers to drop
	// buffers no one references, and never drops a pinned one.
	void img_pin_pixels(void const * data);
	void img_unpin_pixels(void const * data);

	// img_fs.cpp
	img_prop const* img_root_for_path(std::string const& path);
	img_prop* img_node_for_path(std::string const& path, bool directory);
}
