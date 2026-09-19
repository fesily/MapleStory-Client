//////////////////////////////////////////////////////////////////////////////////
//	Loose .img backend: the nl:: API												//
//																				//
//	Implements the NoLifeNx node/bitmap/audio API on top of the .img data		//
//	folder, so the client source stays unchanged. Semantics mirror				//
//	includes/NoLifeNx/nlnx/node.cpp: integer keys are converted to strings,		//
//	missing children yield null nodes, data access converts between types the	//
//	same way, and resolve() splits a path on '/'.								//
//////////////////////////////////////////////////////////////////////////////////
#include "../../MapleStory.h"

#ifdef USE_IMG
#include "img_impl.hpp"

#include <unordered_map>


#include <nlnx/audio.hpp>
#include <nlnx/bitmap.hpp>

#include <cstring>
#include <deque>
#include <sstream>
#include <stdexcept>

namespace
{
	// Decoded canvases are cached; the atlas uploads each bitmap once, but the
	// client keeps nodes around and re-reads bitmaps after an atlas rebuild.
	constexpr size_t DECODED_BUDGET = 256 * 1024 * 1024;

	size_t g_decoded_bytes = 0;

	// Never destroyed (see img_immortal): a texture released from a singleton's
	// destructor still unpins its pixels after this file's globals would be gone.
	std::deque<nl::img_prop const*>& g_decoded_order = nl::img_immortal<std::deque<nl::img_prop const*>>();

	// Identity handed to nl::bitmap: never reused, unlike the address of a buffer
	size_t g_uid = 0;

	// Decoded buffers resources currently draw from (buffer address -> references)
	std::unordered_map<void const*, size_t>& g_pins = nl::img_immortal<std::unordered_map<void const*, size_t>>();

	std::vector<uint8_t> const* decoded_pixels(nl::img_prop const& prop)
	{
		if (!prop.decoded.empty())
			return &prop.decoded;

		if (prop.failed || prop.kind != nl::IMG_CANVAS)
			return nullptr;

		std::vector<uint8_t> pixels = nl::img_canvas_pixels(prop);

		if (pixels.empty())
		{
			prop.failed = true;
			return nullptr;
		}

		while (g_decoded_bytes + pixels.size() > DECODED_BUDGET && !g_decoded_order.empty())
		{
			// Drop the oldest canvas that nothing draws from and drop entries that
			// already lost their pixels; when every buffer left is in use the cache
			// grows over its budget instead of pulling pixels out from under the
			// renderer.
			bool dropped = false;

			for (auto iter = g_decoded_order.begin(); iter != g_decoded_order.end(); ++iter)
			{
				nl::img_prop* victim = const_cast<nl::img_prop*>(*iter);

				if (!victim->decoded.empty() && g_pins.count(victim->decoded.data()) != 0)
					continue;

				if (!victim->decoded.empty())
					g_decoded_bytes -= victim->decoded.size();

				victim->decoded.clear();
				victim->decoded.shrink_to_fit();
				g_decoded_order.erase(iter);
				dropped = true;

				break;
			}

			if (!dropped)
				break;
		}

		prop.decoded = std::move(pixels);
		g_decoded_bytes += prop.decoded.size();
		g_decoded_order.push_back(&prop);

		return &prop.decoded;
	}
}

namespace nl
{
	node img_access::make(img_prop const* p, img_prop const* root)
	{
		img_prop const* effective_root = (p && p->kind >= IMG_DIR && p->root) ? p->root : root;

		if (!p)
			return effective_root ? node(nullptr, &effective_root->fd) : node(nullptr, nullptr);

		img_prop* mutable_prop = const_cast<img_prop*>(p);
		mutable_prop->nd.prop = p;
		mutable_prop->fd.root = effective_root;

		return node(&mutable_prop->nd, &mutable_prop->fd);
	}

	node::node(node const & other) : m_data(other.m_data), m_file(other.m_file) {}
	node::node(data const * d, _file_data const * f) : m_data(d), m_file(f) {}

	node node::begin() const
	{
		if (!m_data)
			return node(nullptr, m_file);

		img_prop const* prop = m_data->prop;
		img_ensure_loaded(prop);

		if (prop->kids.empty())
			return node(nullptr, m_file);

		return img_access::make(prop->kids.front(), m_file ? m_file->root : nullptr);
	}

	node node::end() const
	{
		return node(nullptr, m_file);
	}

	node node::operator*() const
	{
		return *this;
	}

	node & node::operator++()
	{
		if (!m_data)
			return *this;

		img_prop const* prop = m_data->prop;
		img_prop const* parent = prop->parent;

		if (parent && static_cast<size_t>(prop->index) + 1 < parent->kids.size())
			*this = img_access::make(parent->kids[prop->index + 1], m_file ? m_file->root : nullptr);
		else
			*this = node(nullptr, m_file);

		return *this;
	}

	node node::operator++(int)
	{
		node previous = *this;
		++(*this);
		return previous;
	}

	bool node::operator==(node const & other) const
	{
		return m_data == other.m_data;
	}

	bool node::operator!=(node const & other) const
	{
		return m_data != other.m_data;
	}

	bool node::operator<(node const & other) const
	{
		return m_data < other.m_data;
	}

	std::string operator+(std::string s, node n) { return s + n.get_string(); }
	std::string operator+(char const * s, node n) { return s + n.get_string(); }
	std::string operator+(node n, std::string s) { return n.get_string() + s; }
	std::string operator+(node n, char const * s) { return n.get_string() + s; }

	node node::operator[](unsigned int n) const { return operator[](std::to_string(n)); }
	node node::operator[](signed int n) const { return operator[](std::to_string(n)); }
	node node::operator[](unsigned long n) const { return operator[](std::to_string(n)); }
	node node::operator[](signed long n) const { return operator[](std::to_string(n)); }
	node node::operator[](unsigned long long n) const { return operator[](std::to_string(n)); }
	node node::operator[](signed long long n) const { return operator[](std::to_string(n)); }

	node node::operator[](std::string const & o) const
	{
		if (!m_data)
			return node(nullptr, m_file);

		img_prop const* child = img_lookup(m_data->prop, o);
		return img_access::make(child, m_file ? m_file->root : nullptr);
	}

	node node::operator[](char const * o) const
	{
		return operator[](std::string(o ? o : ""));
	}

	node node::operator[](node const & o) const
	{
		return operator[](o.get_string());
	}

	node::operator unsigned char() const { return static_cast<unsigned char>(get_integer()); }
	node::operator signed char() const { return static_cast<signed char>(get_integer()); }
	node::operator unsigned short() const { return static_cast<unsigned short>(get_integer()); }
	node::operator signed short() const { return static_cast<signed short>(get_integer()); }
	node::operator unsigned int() const { return static_cast<unsigned int>(get_integer()); }
	node::operator signed int() const { return static_cast<signed int>(get_integer()); }
	node::operator unsigned long() const { return static_cast<unsigned long>(get_integer()); }
	node::operator signed long() const { return static_cast<signed long>(get_integer()); }
	node::operator unsigned long long() const { return static_cast<unsigned long long>(get_integer()); }
	node::operator signed long long() const { return static_cast<signed long long>(get_integer()); }
	node::operator float() const { return static_cast<float>(get_real()); }
	node::operator double() const { return static_cast<double>(get_real()); }
	node::operator long double() const { return static_cast<long double>(get_real()); }
	node::operator std::string() const { return get_string(); }
	node::operator vector2i() const { return get_vector(); }
	node::operator bitmap() const { return get_bitmap(); }
	node::operator audio() const { return get_audio(); }

	node::operator bool() const
	{
		return m_data ? true : false;
	}

	int64_t node::get_integer(int64_t def) const
	{
		if (!m_data)
			return def;

		img_prop const* prop = m_data->prop;

		switch (prop->kind)
		{
		case IMG_INT:
			return prop->ival;
		case IMG_REAL:
			return static_cast<int64_t>(prop->rval);
		case IMG_STRING:
			return std::stoll(prop->sval);
		default:
			return def;
		}
	}

	double node::get_real(double def) const
	{
		if (!m_data)
			return def;

		img_prop const* prop = m_data->prop;

		switch (prop->kind)
		{
		case IMG_INT:
			return static_cast<double>(prop->ival);
		case IMG_REAL:
			return prop->rval;
		case IMG_STRING:
			return std::stod(prop->sval);
		default:
			return def;
		}
	}

	std::string node::get_string(std::string def) const
	{
		if (!m_data)
			return def;

		img_prop const* prop = m_data->prop;

		switch (prop->kind)
		{
		case IMG_INT:
			return std::to_string(prop->ival);
		case IMG_REAL:
			return std::to_string(prop->rval);
		case IMG_STRING:
			return prop->sval;
		default:
			return def;
		}
	}

	vector2i node::get_vector(vector2i def) const
	{
		if (m_data && m_data->prop->kind == IMG_VECTOR)
			return { m_data->prop->vx, m_data->prop->vy };

		return def;
	}

	bitmap node::get_bitmap() const
	{
		if (!m_data || m_data->prop->kind != IMG_CANVAS)
			return bitmap(nullptr, 0, 0);

		img_prop const* prop = m_data->prop;
		std::vector<uint8_t> const* pixels = decoded_pixels(*prop);

		if (!pixels || pixels->empty())
			return bitmap(nullptr, 0, 0);

		if (prop->uid == 0)
			prop->uid = ++g_uid;

		return bitmap(pixels->data(), prop->width, prop->height, prop->uid);
	}

	void img_pin_pixels(void const * data)
	{
		if (data)
			g_pins[data]++;
	}

	void img_unpin_pixels(void const * data)
	{
		if (!data)
			return;

		auto iter = g_pins.find(data);

		if (iter != g_pins.end() && --iter->second == 0)
			g_pins.erase(iter);
	}

	audio node::get_audio() const
	{
		if (!m_data || m_data->prop->kind != IMG_AUDIO)
			return audio(nullptr, 0);

		img_prop const* prop = m_data->prop;

		// A package keeps its sound data in the mapping, the loose folder in the
		// payload the parser read
		if (prop->blob_ptr)
			return audio(prop->blob_ptr, prop->blob_size);

		if (prop->payload.empty())
			return audio(nullptr, 0);

		return audio(prop->payload.data(), static_cast<uint32_t>(prop->payload.size()));
	}

	bool node::get_bool() const
	{
		return m_data && m_data->prop->kind == IMG_INT && m_data->prop->ival != 0;
	}

	bool node::get_bool(bool def) const
	{
		return m_data && m_data->prop->kind == IMG_INT ? m_data->prop->ival != 0 : def;
	}

	int32_t node::x() const
	{
		return m_data && m_data->prop->kind == IMG_VECTOR ? m_data->prop->vx : 0;
	}

	int32_t node::y() const
	{
		return m_data && m_data->prop->kind == IMG_VECTOR ? m_data->prop->vy : 0;
	}

	std::string node::name() const
	{
		return m_data ? m_data->prop->name : std::string();
	}

	size_t node::size() const
	{
		if (!m_data)
			return 0;

		img_prop const* prop = m_data->prop;

		if (img_has_children(prop))
			img_ensure_loaded(prop);

		return prop->kids.size();
	}

	node::type node::data_type() const
	{
		if (!m_data)
			return type::none;

		switch (m_data->prop->kind)
		{
		case IMG_INT:
			return type::integer;
		case IMG_REAL:
			return type::real;
		case IMG_STRING:
			return type::string;
		case IMG_VECTOR:
			return type::vector;
		case IMG_CANVAS:
			return type::bitmap;
		case IMG_AUDIO:
			return type::audio;
		default:
			return type::none;
		}
	}

	node node::root() const
	{
		if (!m_file || !m_file->root)
			return node(nullptr, m_file);

		return img_access::make(m_file->root, nullptr);
	}

	node node::resolve(std::string path) const
	{
		std::istringstream stream(path);
		std::string segment;
		node resolved = *this;

		while (std::getline(stream, segment, '/'))
			resolved = resolved[segment];

		return resolved;
	}

	bitmap::bitmap(void const * data, uint16_t width, uint16_t height)
		: bitmap(data, width, height, 0) {}

	bitmap::bitmap(void const * data, uint16_t width, uint16_t height, size_t uid)
		: m_data(data), m_width(width), m_height(height), m_uid(uid) {}

	bitmap::operator bool() const { return m_data ? true : false; }
	bool bitmap::operator==(bitmap const & other) const { return m_data == other.m_data; }
	bool bitmap::operator<(bitmap const & other) const { return m_data < other.m_data; }
	void const * bitmap::data() const { return m_data; }
	uint16_t bitmap::width() const { return m_width; }
	uint16_t bitmap::height() const { return m_height; }
	uint32_t bitmap::length() const { return 4u * m_width * m_height; }
	size_t bitmap::id() const { return m_uid; }

	audio::audio(void const * data, uint32_t length)
		: m_data(data), m_length(length) {}

	audio::operator bool() const { return m_data ? true : false; }
	bool audio::operator==(audio const & other) const { return m_data == other.m_data; }
	bool audio::operator<(audio const & other) const { return m_data < other.m_data; }
	void const * audio::data() const { return m_data; }
	uint32_t audio::length() const { return m_length; }
	size_t audio::id() const { return reinterpret_cast<size_t>(m_data); }

	namespace nx
	{
		node Base, Character, Effect, Etc, Item, Map, Map001, Map002, Map2, Mob, Mob001, Mob002, Mob2, Morph, Npc, Quest, Reactor, Skill, Skill001, Skill002, Skill003, Sound, Sound001, Sound002, Sound2, String, TamingMob, UI;

		void load_all()
		{
			img_load_roots();

			Base = img_root("Base");
			Character = img_root("Character");
			Effect = img_root("Effect");
			Etc = img_root("Etc");
			Item = img_root("Item");
			Map = img_root("Map");
			Map001 = img_root("Map001");
			Map002 = img_root("Map002");
			Map2 = img_root("Map2");
			Mob = img_root("Mob");
			Mob001 = img_root("Mob001");
			Mob002 = img_root("Mob002");
			Mob2 = img_root("Mob2");
			Morph = img_root("Morph");
			Npc = img_root("Npc");
			Quest = img_root("Quest");
			Reactor = img_root("Reactor");
			Skill = img_root("Skill");
			Skill001 = img_root("Skill001");
			Skill002 = img_root("Skill002");
			Skill003 = img_root("Skill003");
			Sound = img_root("Sound");
			Sound001 = img_root("Sound001");
			Sound002 = img_root("Sound002");
			Sound2 = img_root("Sound2");
			String = img_root("String");
			TamingMob = img_root("TamingMob");
			UI = img_root("UI");
		}
	}
}

#endif // USE_IMG
