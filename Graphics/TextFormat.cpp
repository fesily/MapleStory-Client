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
#include "TextFormat.h"

#include <cstring>

namespace ms
{
	namespace single_console
	{
		// Declared in Util/Misc.h; repeated here so that this module does not have to
		// pull in the headers of the UI it is used by
		void log_message(std::string message);
	}
}

namespace
{
	// Style codes. They switch the state of the text and are always exactly two
	// bytes: the official parser compares the whole phrase against them, they take
	// no payload and no '#'.
	constexpr const char* STYLE_CODES = "krgbdenl";

	// Two byte codes which draw no text: the quest markers #E #I #S #K, the reward
	// mark #w, ## (the official parser swallows both bytes and emits nothing) and #x,
	// the code the parser no longer gives a meaning to
	constexpr const char* MARKER_CODES = "EISKw#x";

	// Payload codes whose payload ends at a '#', which is consumed and not drawn. A
	// '\', a CR and the end of the text end it as well: the official parser scans for
	// those and drops what it finds, so an unterminated code swallows the rest of the
	// text there, and so it does here.
	constexpr const char* PREFIX_CODES = "@BFLM_acfhimopqstuvyz";

	// Payload codes whose payload may contain a '\' or a CR: everything up to the
	// next '#' belongs to the code, the '#' included. Without a '#' the code stays
	// two bytes long.
	constexpr const char* SCAN_CODES = "DQRWj";

	// What ends a run of plain characters. The space and the tab also separate words
	// for the layout, the CR and the '\' are line breaks of their own. The control
	// characters are listed one by one so that a run stops where they start: the
	// parser draws nothing for them, so a lone LF of a text written with LF endings
	// must not reach the font as a character
	constexpr const char* PLAIN_TERMINATORS = " \\#\t\r\x01\x02\x03\x04\x05\x06\x07\x08\x0a\x0b\x0c\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f";

	// What ends a payload of a PREFIX_CODES code
	constexpr const char* PAYLOAD_TERMINATORS = "#\\\r";
}

namespace ms
{
	namespace textformat
	{
		// The dialect of the descriptions CUIToolTip draws (DrawTextSepartedLine at
		// 0x890490): a '#' (with the 'c' that follows it) is a prefix which is not
		// drawn, an escape takes two bytes and only '\n' (a new line) and '\\' (one
		// backslash) do anything, a real LF starts a new line as well, and the text of
		// a description is otherwise made of characters, so a piece of it ends where
		// the layout may break it (see FormatText)
		static Unit classify_description(const std::string& text, size_t index, size_t& length, uint8_t first)
		{
			if (first == '#' && index + 1 >= text.size())
			{
				// A '#' at the very end of a description is a prefix without anything
				// behind it: the tooltip renderer never draws it
				length = 1;

				return Unit::MARKER;
			}

			if (first == '#' && index + 1 < text.size())
			{
				char code = text[index + 1];

				// The styles of a description are the codes a dialog uses
				if (std::strchr(STYLE_CODES, code))
				{
					length = 2;

					return Unit::STYLE;
				}

				// Every other code keeps the meaning it has in a dialog, so that a name
				// or a canvas a description asks for is not lost. A code which is not
				// terminated is no code here: '#c...' is the prefix of a description.
				if (code != 'c' && (std::strchr(PREFIX_CODES, code) || std::strchr(SCAN_CODES, code)))
				{
					Unit unit = classify(text, index, length, Mode::DIALOG);

					if (unit == Unit::PAYLOAD && text[index + length - 1] == '#')
						return unit;

					length = 1;

					return Unit::MARKER;
				}

				// a '#' takes the 'c' behind it along, a lone one only itself
				length = code == 'c' ? 2 : 1;

				return Unit::MARKER;
			}

			if (first == '\\' && index + 1 >= text.size())
			{
				// an escape without a character behind it ends the description
				length = 1;

				return Unit::MARKER;
			}

			if (first == '\\' && index + 1 < text.size())
			{
				char escaped = text[index + 1];

				if (escaped == 'n' || escaped == 'N')
				{
					length = 2;

					return Unit::BREAK;
				}

				if (escaped == '\\')
				{
					// one backslash; the analyzer hands it to the text as it stands
					length = 2;

					return Unit::LITERAL;
				}

				// every other escape of a description is dropped, the character it
				// escaped with included
				size_t charlen = 0;
				utf8_decode(text, index + 1, charlen);

				length = 1 + charlen;

				return Unit::MARKER;
			}

			if (first == '\n' || first == '\r')
			{
				length = first == '\r' && index + 1 < text.size() && text[index + 1] == '\n' ? 2 : 1;

				return Unit::BREAK;
			}

			size_t end = text.find_first_of(" \\#\t\r\n", index + 1);

			length = (end == std::string::npos ? text.size() : end) - index;

			return Unit::PLAIN;
		}

		Unit classify(const std::string& text, size_t index, size_t& length, Mode mode)
		{
			uint8_t first = static_cast<uint8_t>(text[index]);

			if (mode == Mode::DESCRIPTION)
				return classify_description(text, index, length, first);

			if (first == '#' && index + 1 < text.size())
			{
				char code = text[index + 1];

				if (std::strchr(STYLE_CODES, code))
				{
					length = 2;

					return Unit::STYLE;
				}

				if (std::strchr(MARKER_CODES, code))
				{
					length = 2;

					return Unit::MARKER;
				}

				if (std::strchr(PREFIX_CODES, code))
				{
					size_t end = index + 2;

					while (end < text.size() && std::strchr(PAYLOAD_TERMINATORS, text[end]) == nullptr)
						end++;

					if (end == text.size())
					{
						// The official parser swallows the rest of the text when a code
						// is not terminated. The layout does the same, so a payload can
						// never leak into the text, but it is worth a note. Only the code
						// is logged, so a text that is revealed character by character
						// reports it once instead of once per step.
						single_console::log_message("[TextFormat::classify] Unterminated format code: [" + text.substr(index, 2) + "]");
					}
					else
					{
						// The terminator is consumed and not drawn
						end++;
					}

					length = end - index;

					return Unit::PAYLOAD;
				}

				if (std::strchr(SCAN_CODES, code))
				{
					size_t end = text.find('#', index + 2);

					length = end == std::string::npos ? 2 : end - index + 1;

					return Unit::PAYLOAD;
				}

				// A '#' which starts no code is not a code at all: the official parser
				// takes the '#' and the character which follows it, so that a multi byte
				// character is never split between two units
				size_t charlen = 0;
				utf8_decode(text, index + 1, charlen);

				length = 1 + charlen;

				return Unit::LITERAL;
			}

			if (first == '\\')
			{
				// An escape takes the character it escapes with it, and a lone '\' at
				// the end of the text is a line break of its own
				size_t charlen = 0;

				if (index + 1 < text.size())
					utf8_decode(text, index + 1, charlen);

				length = 1 + charlen;

				return Unit::BREAK;
			}

			if (first == '\r')
			{
				// The official parser drops one character after a CR, which is the LF
				// when the text was written with CRLF. Only the LF is dropped here: an
				// arbitrary character would be lost instead of drawn.
				length = index + 1 < text.size() && text[index + 1] == '\n' ? 2 : 1;

				return Unit::BREAK;
			}

			if (first < 0x20 && first != '\t')
			{
				// The rest of the control characters draw nothing: a lone LF of a text
				// written with LF endings and the backspace the format table lists are
				// the ones which occur in the texts a server sends
				length = 1;

				return Unit::MARKER;
			}

			size_t end = text.find_first_of(PLAIN_TERMINATORS, index + 1);

			length = (end == std::string::npos ? text.size() : end) - index;

			return Unit::PLAIN;
		}

		std::string payload(const std::string& text, size_t index, size_t length)
		{
			// The payload runs from behind the code up to the terminator the scanner
			// consumed, which is dropped: it is the '#' the text continues with, or the
			// '\', the CR or the end of the text a scanned code stops at
			size_t first = index + 2;
			size_t last = index + length;

			if (last > first && std::strchr(PAYLOAD_TERMINATORS, text[last - 1]))
				last--;

			return text.substr(first, last - first);
		}

		uint32_t utf8_decode(const std::string& text, size_t index, size_t& length)
		{
			uint8_t lead = static_cast<uint8_t>(text[index]);

			if (lead < 0x80)
			{
				length = 1;

				return lead;
			}

			size_t count;
			uint32_t codepoint;

			if ((lead & 0xE0) == 0xC0)
			{
				count = 2;
				codepoint = lead & 0x1F;
			}
			else if ((lead & 0xF0) == 0xE0)
			{
				count = 3;
				codepoint = lead & 0x0F;
			}
			else if ((lead & 0xF8) == 0xF0)
			{
				count = 4;
				codepoint = lead & 0x07;
			}
			else
			{
				length = 1;

				return 0xFFFD;
			}

			if (index + count > text.size())
			{
				length = 1;

				return 0xFFFD;
			}

			for (size_t i = 1; i < count; i++)
			{
				uint8_t next = static_cast<uint8_t>(text[index + i]);

				if ((next & 0xC0) != 0x80)
				{
					length = 1;

					return 0xFFFD;
				}

				codepoint = (codepoint << 6) | (next & 0x3F);
			}

			length = count;

			return codepoint;
		}
	}
}
