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
#include "InPacket.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include <string>

namespace
{
	// The server writes every string field in the charset of the client's language
	// (org.gms.constants.string.CharsetConstants): "GBK" for zh-CN, "US-ASCII" for
	// en-US. GBK is a superset of ASCII, so decoding as GBK is correct for both.
	// The conversion uses the Win32 API (code page 936) to avoid adding a
	// dependency. It never changes how many bytes are read from the packet: the
	// length prefix is a byte count and the whole field is consumed before the
	// conversion happens.
	const unsigned int GBK_CODEPAGE = 936;
	const unsigned int UTF8_CODEPAGE = 65001;

	bool is_ascii(const std::string& text)
	{
		for (char c : text)
		{
			if (static_cast<uint8_t>(c) >= 0x80)
				return false;
		}

		return true;
	}

	// Remove a trailing GBK lead byte which has no trail byte. The server truncates
	// fixed-length strings on a byte boundary (writeFixedString), so a multibyte
	// character can be cut in half; the remaining byte cannot be decoded.
	void strip_incomplete_tail(std::string& text)
	{
		size_t size = text.size();
		size_t pos = 0;

		while (pos < size)
		{
			uint8_t lead = static_cast<uint8_t>(text[pos]);

			if (lead < 0x80)
			{
				pos++;
				continue;
			}

			if (lead >= 0x81 && lead <= 0xFE)
			{
				// Lead byte of a two byte sequence
				if (pos + 1 == size)
				{
					text.resize(size - 1);
					return;
				}

				pos += 2;
				continue;
			}

			// Lone 0x80 or 0xFF: leave it to the decoder
			pos++;
		}
	}

	// Convert a GBK byte string to UTF-8. Invalid input is replaced by the default
	// character of the code page, and the bytes are passed through unchanged when
	// the conversion is unavailable.
	std::string gbk_to_utf8(std::string text)
	{
		// Fast path: ASCII is identical in GBK and UTF-8
		if (is_ascii(text))
			return text;

		strip_incomplete_tail(text);

#ifdef _WIN32
		int length = static_cast<int>(text.size());
		int wide_length = MultiByteToWideChar(GBK_CODEPAGE, 0, text.data(), length, nullptr, 0);

		if (wide_length <= 0)
			return text;

		std::wstring wide(wide_length, L'\0');

		if (MultiByteToWideChar(GBK_CODEPAGE, 0, text.data(), length, &wide[0], wide_length) != wide_length)
			return text;

		int utf8_length = WideCharToMultiByte(UTF8_CODEPAGE, 0, wide.data(), wide_length, nullptr, 0, nullptr, nullptr);

		if (utf8_length <= 0)
			return text;

		std::string utf8(utf8_length, '\0');

		if (WideCharToMultiByte(UTF8_CODEPAGE, 0, wide.data(), wide_length, &utf8[0], utf8_length, nullptr, nullptr) != utf8_length)
			return text;

		return utf8;
#else
		return text;
#endif
	}
}

namespace ms
{
	InPacket::InPacket(const int8_t* recv, size_t length)
	{
		bytes = recv;
		top = length;
		pos = 0;
	}

	bool InPacket::available() const
	{
		return length() > 0;
	}

	size_t InPacket::length() const
	{
		return top - pos;
	}

	void InPacket::skip(size_t count)
	{
		if (count > length())
			throw PacketError("Stack underflow at " + std::to_string(pos));

		pos += count;
	}

	bool InPacket::read_bool()
	{
		return read_byte() == 1;
	}

	int8_t InPacket::read_byte()
	{
		return read<int8_t>();
	}

	int16_t InPacket::read_short()
	{
		return read<int16_t>();
	}

	int32_t InPacket::read_int()
	{
		return read<int32_t>();
	}

	int64_t InPacket::read_long()
	{
		return read<int64_t>();
	}

	Point<int16_t> InPacket::read_point()
	{
		int16_t x = read<int16_t>();
		int16_t y = read<int16_t>();

		return Point<int16_t>(x, y);
	}

	std::string InPacket::read_string()
	{
		uint16_t length = read<uint16_t>();

		return read_padded_string(length);
	}

	std::string InPacket::read_padded_string(uint16_t count)
	{
		std::string ret;

		for (int16_t i = 0; i < count; i++)
		{
			char letter = read_byte();

			if (letter != '\0')
				ret.push_back(letter);
		}

		// The bytes arrive in the charset of the server's client language (GBK),
		// the rest of the client works with UTF-8. The field has already been
		// consumed, so the conversion cannot shift the packet position.
		return gbk_to_utf8(ret);
	}

	void InPacket::skip_bool()
	{
		skip_byte();
	}

	void InPacket::skip_byte()
	{
		skip(sizeof(int8_t));
	}

	void InPacket::skip_short()
	{
		skip(sizeof(int16_t));
	}

	void InPacket::skip_int()
	{
		skip(sizeof(int32_t));
	}

	void InPacket::skip_long()
	{
		skip(sizeof(int64_t));
	}

	void InPacket::skip_point()
	{
		skip(sizeof(int16_t));
		skip(sizeof(int16_t));
	}

	void InPacket::skip_string()
	{
		uint16_t length = read<uint16_t>();

		skip_padded_string(length);
	}

	void InPacket::skip_padded_string(uint16_t length)
	{
		skip(length);
	}

	bool InPacket::inspect_bool()
	{
		return inspect_byte() == 1;
	}

	int8_t InPacket::inspect_byte()
	{
		return inspect<int8_t>();
	}

	int16_t InPacket::inspect_short()
	{
		return inspect<int16_t>();
	}

	int32_t InPacket::inspect_int()
	{
		return inspect<int32_t>();
	}

	int64_t InPacket::inspect_long()
	{
		return inspect<int64_t>();
	}
}