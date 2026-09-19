//////////////////////////////////////////////////////////////////////////////////
//	Standalone .img parser														//
//																				//
//	An .img file is a plain WZ image block located at offset 0:					//
//																				//
//		0x73 "Property" u16(0)                 image header					//
//		u8 count, then count properties:										//
//			<name string block> <type byte> <payload>							//
//																				//
//	Strings are XOR encrypted with the WZ string key (standard MapleStory AES	//
//	user key with the IV of the client version - GMS here), see img_keystream().	//
//	Property layout follows MapleLib's WzImageProperty.ParsePropertyList.		//
//////////////////////////////////////////////////////////////////////////////////
#include "../../MapleStory.h"

#ifdef USE_IMG
#include "img_impl.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
	using nl::img_kind;
	using nl::img_prop;

	constexpr size_t MAX_DEPTH = 64;
	constexpr size_t MAX_STRING = 1 << 22;

	std::string utf16_to_utf8(uint16_t const* w, size_t count)
	{
		std::string out;
		out.reserve(count);

		for (size_t i = 0; i < count; i++)
		{
			uint32_t code = w[i];

			if (code >= 0xD800 && code <= 0xDBFF && i + 1 < count && w[i + 1] >= 0xDC00 && w[i + 1] <= 0xDFFF)
				code = 0x10000 + ((code - 0xD800) << 10) + (w[++i] - 0xDC00);

			if (code < 0x80)
			{
				out.push_back(static_cast<char>(code));
			}
			else if (code < 0x800)
			{
				out.push_back(static_cast<char>(0xC0 | (code >> 6)));
				out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
			else if (code < 0x10000)
			{
				out.push_back(static_cast<char>(0xE0 | (code >> 12)));
				out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
			else
			{
				out.push_back(static_cast<char>(0xF0 | (code >> 18)));
				out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
			}
		}

		return out;
	}

	class img_reader
	{
	public:
		img_reader(uint8_t const* bytes, size_t size) : bytes(bytes), size(size) {}

		uint8_t const* data() const { return bytes; }
		size_t pos() const { return offset; }
		size_t remaining() const { return offset <= size ? size - offset : 0; }
		void seek(size_t to) { offset = to; }

		bool u8(uint8_t& out)
		{
			if (remaining() < 1)
				return false;

			out = bytes[offset++];
			return true;
		}

		bool i8(int8_t& out)
		{
			uint8_t byte;

			if (!u8(byte))
				return false;

			out = static_cast<int8_t>(byte);
			return true;
		}

		bool i16(int16_t& out)
		{
			if (remaining() < 2)
				return false;

			out = static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8));
			offset += 2;
			return true;
		}

		bool u16(uint16_t& out)
		{
			if (remaining() < 2)
				return false;

			out = static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
			offset += 2;
			return true;
		}

		bool i32(int32_t& out)
		{
			if (remaining() < 4)
				return false;

			out = static_cast<int32_t>(
				static_cast<uint32_t>(bytes[offset])
				| (static_cast<uint32_t>(bytes[offset + 1]) << 8)
				| (static_cast<uint32_t>(bytes[offset + 2]) << 16)
				| (static_cast<uint32_t>(bytes[offset + 3]) << 24)
				);
			offset += 4;
			return true;
		}

		bool u32(uint32_t& out)
		{
			int32_t value;

			if (!i32(value))
				return false;

			out = static_cast<uint32_t>(value);
			return true;
		}

		bool i64(int64_t& out)
		{
			if (remaining() < 8)
				return false;

			uint64_t value = 0;

			for (int i = 0; i < 8; i++)
				value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);

			offset += 8;
			out = static_cast<int64_t>(value);
			return true;
		}

		bool f32(float& out)
		{
			int32_t raw;

			if (!i32(raw))
				return false;

			std::memcpy(&out, &raw, sizeof(out));
			return true;
		}

		bool f64(double& out)
		{
			if (remaining() < 8)
				return false;

			uint64_t raw = 0;

			for (int i = 0; i < 8; i++)
				raw |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);

			offset += 8;
			std::memcpy(&out, &raw, sizeof(out));
			return true;
		}

		bool skip(size_t count)
		{
			if (remaining() < count)
				return false;

			offset += count;
			return true;
		}

		// WzBinaryReader.ReadCompressedInt
		bool compressed_int(int32_t& out)
		{
			int8_t small;

			if (!i8(small))
				return false;

			if (small != -128)
			{
				out = small;
				return true;
			}

			return i32(out);
		}

		// WzBinaryReader.ReadString
		bool read_string(std::string& out)
		{
			int8_t length;

			if (!i8(length))
				return false;

			if (length == 0)
			{
				out.clear();
				return true;
			}

			bool unicode = length > 0;
			int32_t count = 0;

			if (length > 0)
			{
				if (length == 127)
				{
					if (!i32(count))
						return false;
				}
				else
				{
					count = length;
				}
			}
			else
			{
				if (length == -128)
				{
					if (!i32(count))
						return false;
				}
				else
				{
					count = -length;
				}
			}

			if (count < 0 || static_cast<size_t>(count) > MAX_STRING)
				return false;

			size_t char_size = unicode ? 2 : 1;
			size_t key_length = static_cast<size_t>(count) * char_size;

			if (key_length > 0 && nl::img_keystream(key_length).size() < key_length)
				return false;

			auto const& key = nl::img_keystream(key_length);

			if (unicode)
			{
				if (remaining() < key_length)
					return false;

				std::vector<uint16_t> chars(static_cast<size_t>(count));
				uint16_t mask = 0xAAAA;

				for (int32_t i = 0; i < count; i++)
				{
					uint16_t raw;

					if (!u16(raw))
						return false;

					raw ^= mask++;
					raw ^= static_cast<uint16_t>((key[i * 2 + 1] << 8) + key[i * 2]);
					chars[static_cast<size_t>(i)] = raw;
				}

				out = utf16_to_utf8(chars.data(), chars.size());
			}
			else
			{
				if (remaining() < key_length)
					return false;

				out.resize(static_cast<size_t>(count));
				uint8_t mask = 0xAA;

				for (int32_t i = 0; i < count; i++)
				{
					uint8_t raw;

					if (!u8(raw))
						return false;

					out[static_cast<size_t>(i)] = static_cast<char>(raw ^ mask++ ^ key[static_cast<size_t>(i)]);
				}
			}

			return true;
		}

		bool read_string_at(size_t at, std::string& out)
		{
			if (at > size)
				return false;

			size_t saved = offset;
			offset = at;
			bool ok = read_string(out);
			offset = saved;
			return ok;
		}

		// WzBinaryReader.ReadStringBlock
		bool read_string_block(size_t image_base, std::string& out)
		{
			uint8_t header;

			if (!u8(header))
				return false;

			if (header == 0 || header == 0x73)
				return read_string(out);

			if (header == 1 || header == 0x1B)
			{
				int32_t relative;

				if (!i32(relative))
					return false;

				return read_string_at(image_base + static_cast<size_t>(relative), out);
			}

			return false;
		}

	private:
		uint8_t const* bytes;
		size_t size;
		size_t offset = 0;
	};

	bool parse_property_list(img_reader& reader, size_t base, std::vector<std::unique_ptr<img_prop>>& out, size_t depth);

	bool parse_extended(img_reader& reader, size_t base, std::string name, img_prop*& out, size_t depth);

	bool parse_property(img_reader& reader, size_t base, std::string name, uint8_t type, img_prop*& out, size_t depth)
	{
		std::unique_ptr<img_prop> prop(new img_prop());
		prop->name = std::move(name);

		switch (type)
		{
		case 0:
			prop->kind = nl::IMG_NULL;
			break;
		case 2:
		case 11:
		{
			int16_t value;

			if (!reader.i16(value))
				return false;

			prop->kind = nl::IMG_INT;
			prop->ival = value;
			break;
		}
		case 3:
		case 19:
		{
			int32_t value;

			if (!reader.compressed_int(value))
				return false;

			prop->kind = nl::IMG_INT;
			prop->ival = value;
			break;
		}
		case 20:
		{
			int64_t value;

			if (!reader.i64(value))
				return false;

			prop->kind = nl::IMG_INT;
			prop->ival = value;
			break;
		}
		case 4:
		{
			uint8_t subtype;

			if (!reader.u8(subtype))
				return false;

			prop->kind = nl::IMG_REAL;

			if (subtype == 0x80)
			{
				float value;

				if (!reader.f32(value))
					return false;

				prop->rval = value;
			}
			else if (subtype != 0)
			{
				return false;
			}

			break;
		}
		case 5:
		{
			double value;

			if (!reader.f64(value))
				return false;

			prop->kind = nl::IMG_REAL;
			prop->rval = value;
			break;
		}
		case 8:
		{
			std::string value;

			if (!reader.read_string_block(base, value))
				return false;

			prop->kind = nl::IMG_STRING;
			prop->sval = std::move(value);
			break;
		}
		case 9:
		{
			uint32_t block_length;

			if (!reader.u32(block_length) || block_length > reader.remaining())
				return false;

			size_t block_end = reader.pos() + block_length;
			img_prop* extended = nullptr;

			if (!parse_extended(reader, base, prop->name, extended, depth))
				return false;

			reader.seek(block_end);
			prop.reset(extended);
			break;
		}
		default:
			return false;
		}

		out = prop.release();
		return true;
	}

	bool parse_property_list(img_reader& reader, size_t base, std::vector<std::unique_ptr<img_prop>>& out, size_t depth)
	{
		if (depth > MAX_DEPTH)
			return false;

		int32_t count;

		if (!reader.compressed_int(count) || count < 0 || count > (1 << 21))
			return false;

		out.reserve(static_cast<size_t>(count));

		for (int32_t i = 0; i < count; i++)
		{
			std::string name;

			if (!reader.read_string_block(base, name))
				return false;

#ifdef _DEBUG
			// Traces the first properties of an image to help diagnosing broken data
			static int trace_budget = 16;

			if (trace_budget > 0)
			{
				char line[192];
				std::snprintf(line, sizeof(line), "prop@0x%zX depth %zu \"%s\"", reader.pos(), depth, name.c_str());
				trace_budget--;
				LOG(LOG_DEBUG, line);
			}
#endif

			uint8_t type;

			if (!reader.u8(type))
				return false;

			img_prop* prop = nullptr;

			if (!parse_property(reader, base, std::move(name), type, prop, depth))
				return false;

			prop->index = static_cast<uint32_t>(out.size());
			out.emplace_back(prop);
		}

		return true;
	}

	bool parse_extended(img_reader& reader, size_t base, std::string name, img_prop*& out, size_t depth)
	{
		if (depth > MAX_DEPTH)
			return false;

		uint8_t header;

		if (!reader.u8(header))
			return false;

		std::string type_name;

		if (header == 1 || header == 0x1B)
		{
			int32_t relative;

			if (!reader.i32(relative))
				return false;

			if (!reader.read_string_at(base + static_cast<size_t>(relative), type_name))
				return false;
		}
		else if (header == 0 || header == 0x73)
		{
			if (!reader.read_string(type_name))
				return false;
		}
		else
		{
			return false;
		}

		if (type_name == "Property")
		{
			if (!reader.skip(2))
				return false;

			std::vector<std::unique_ptr<img_prop>> kids;

			if (!parse_property_list(reader, base, kids, depth + 1))
				return false;

			img_prop* prop = new img_prop();
			prop->name = std::move(name);
			prop->kind = nl::IMG_SUB;
			nl::img_adopt_children(prop, std::move(kids));
			out = prop;
			return true;
		}

		if (type_name == "Canvas")
		{
			// 1 unknown byte, then a flag telling whether sub properties follow
			if (!reader.skip(1))
				return false;

			uint8_t nested;

			if (!reader.u8(nested))
				return false;

			std::vector<std::unique_ptr<img_prop>> kids;

			if (nested == 1)
			{
				if (!reader.skip(2))
					return false;

				if (!parse_property_list(reader, base, kids, depth + 1))
					return false;
			}

			int32_t width, height, format_low, format_high;

			if (!reader.compressed_int(width) || !reader.compressed_int(height)
				|| !reader.compressed_int(format_low) || !reader.compressed_int(format_high))
				return false;

			if (!reader.skip(4))
				return false;

			int32_t encoded_length;

			if (!reader.i32(encoded_length))
				return false;

			uint8_t marker;

			if (!reader.u8(marker))
				return false;

			// The encoded length includes the marker byte
			size_t length = encoded_length > 0 ? static_cast<size_t>(encoded_length) - 1 : 0;

			if (length > reader.remaining())
				return false;

			img_prop* prop = new img_prop();
			prop->name = std::move(name);
			prop->kind = nl::IMG_CANVAS;
			prop->width = width <= 0 ? 0 : static_cast<uint16_t>(std::min(width, 0xFFFF));
			prop->height = height <= 0 ? 0 : static_cast<uint16_t>(std::min(height, 0xFFFF));
			prop->fmt = format_low + (format_high << 8);
			prop->payload.assign(reader.data() + reader.pos(), reader.data() + reader.pos() + length);

			if (!reader.skip(length))
				return false;

			nl::img_adopt_children(prop, std::move(kids));
			out = prop;
			return true;
		}

		if (type_name == "Shape2D#Vector2D")
		{
			int32_t x, y;

			if (!reader.compressed_int(x) || !reader.compressed_int(y))
				return false;

			img_prop* prop = new img_prop();
			prop->name = std::move(name);
			prop->kind = nl::IMG_VECTOR;
			prop->vx = x;
			prop->vy = y;
			out = prop;
			return true;
		}

		if (type_name == "Shape2D#Convex2D")
		{
			int32_t count;

			if (!reader.compressed_int(count) || count < 0 || count > (1 << 16))
				return false;

			std::vector<std::unique_ptr<img_prop>> kids;

			for (int32_t i = 0; i < count; i++)
			{
				img_prop* point = nullptr;

				if (!parse_extended(reader, base, name, point, depth + 1))
					return false;

				kids.emplace_back(point);
			}

			img_prop* prop = new img_prop();
			prop->name = std::move(name);
			prop->kind = nl::IMG_SUB;
			nl::img_adopt_children(prop, std::move(kids));
			out = prop;
			return true;
		}

		if (type_name == "Sound_DX8")
		{
			uint8_t skip;

			if (!reader.u8(skip))
				return false;

			int32_t data_length, length_ms;

			if (!reader.compressed_int(data_length) || !reader.compressed_int(length_ms) || data_length < 0)
				return false;

			size_t header_at = reader.pos();

			if (reader.remaining() < 52)
				return false;

			// 51 byte sound header, 1 unknown byte, then the wave format block
			size_t header_length = 51 + 1 + reader.data()[header_at + 51];

			if (reader.remaining() < header_length + static_cast<size_t>(data_length))
				return false;

			img_prop* prop = new img_prop();
			prop->name = std::move(name);
			prop->kind = nl::IMG_AUDIO;
			prop->payload.assign(
				reader.data() + header_at,
				reader.data() + header_at + header_length + static_cast<size_t>(data_length)
				);
			prop->ival = length_ms; // length in milliseconds

			if (!reader.skip(header_length + static_cast<size_t>(data_length)))
				return false;

			out = prop;
			return true;
		}

		if (type_name == "UOL")
		{
			uint8_t skip;

			if (!reader.u8(skip))
				return false;

			uint8_t subtype;

			if (!reader.u8(subtype))
				return false;

			std::string target;

			if (subtype == 0)
			{
				if (!reader.read_string(target))
					return false;
			}
			else if (subtype == 1)
			{
				int32_t relative;

				if (!reader.i32(relative))
					return false;

				if (!reader.read_string_at(base + static_cast<size_t>(relative), target))
					return false;
			}
			else
			{
				return false;
			}

			img_prop* prop = new img_prop();
			prop->name = std::move(name);
			prop->kind = nl::IMG_STRING;
			prop->sval = std::move(target);
			out = prop;
			return true;
		}

		return false;
	}
}

namespace nl
{
	void img_adopt_children(img_prop* parent, std::vector<std::unique_ptr<img_prop>> kids)
	{
		parent->owned = std::move(kids);
		parent->kids.clear();
		parent->kids.reserve(parent->owned.size());

		for (uint32_t i = 0; i < parent->owned.size(); i++)
		{
			parent->owned[i]->parent = parent;
			parent->owned[i]->index = i;
			parent->kids.push_back(parent->owned[i].get());
		}
	}

	// Parses a standalone image file. Returns false when the file is malformed or
	// uses a property type this backend does not know about.
	bool img_parse(std::string const& path, std::vector<std::unique_ptr<img_prop>>& out)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);

		if (!file.good())
			return false;

		std::streamoff size = file.tellg();

		if (size < 8)
			return false;

		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> bytes(static_cast<size_t>(size));

		if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
			return false;

		img_reader reader(bytes.data(), bytes.size());

		uint8_t header;

		if (!reader.u8(header))
			return false;

		if (header != 0x73) // 0x01 is a lua image, anything else is not supported
			return false;

		std::string magic;
		uint16_t zero;

		if (!reader.read_string(magic) || magic != "Property" || !reader.u16(zero) || zero != 0)
			return false;

		std::vector<std::unique_ptr<img_prop>> props;

		if (!parse_property_list(reader, 0, props, 0))
		{
			char buffer[64];
			std::snprintf(buffer, sizeof(buffer), " (0x%zX)", reader.pos());
			nl::img_log_error("Could not parse image file: " + path + buffer);
			return false;
		}

		out = std::move(props);
		return true;
	}
}

#endif // USE_IMG
