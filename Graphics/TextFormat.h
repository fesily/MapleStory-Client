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
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ms
{
	// Inline formatting of the texts the game and the server produce. The dialect is
	// the one the official client parses (GMS v95, CTextAnalyzer): a text is a
	// sequence of units, and every unit is either plain characters, a format code or
	// an escape. The codes are removed from the text before it is measured and drawn,
	// so their bytes never take part in the layout.
	//
	//	style codes, exactly two bytes, they switch the state of the text:
	//		#k #r #g #b #d		black (base colour), red, green, blue, purple
	//		#e #n				bold on, bold off
	//		#l					ends the select range of a #L entry
	//
	// The official client also has a #e<id> branch which draws an outlined item, but
	// it is unreachable: a code without a '#' after it stops after two bytes, so '#e'
	// is always the bold switch.
	//
	//	markers, exactly two bytes, they draw no text:
	//		#E #I #S #K			quest markers
	//		#w					reward mark
	//		##					a pair the official parser swallows, it emits nothing
	//
	//	payload codes, written as #<code><payload>. The payload ends at the '#', which
	//	is consumed and not drawn; the codes marked with a star in the list below scan
	//	for the '#' and swallow a CR and a '\' inside the payload, the others end at a
	//	'#' as well but also at a '\', a CR or the end of the text:
	//		#t<id> #z<id>		item name			#i<id> #v<id>		item icon and name
	//		#o<id>				monster name		#p<id> #@<id>		NPC name
	//		#m<id>				map name			#q<id>				skill name
	//		#y<id>				quest name			#s<id>				skill icon and name
	//		#L<n>				select entry		#h[1|2|3]			player name
	//		#a<id> #c<id>		kill / item count	#u<id>				quest state
	//		#x<id>				quest bonus exp		#_<key>				hidden key, draws nothing
	//		#B<n>				progress bar		#F #f<path>			inline image
	//		#W<key>				quest summary icon	#M<id>				quest mob name
	//		*#j<payload>		record block		*#D #Q #R			quest records
	//
	//	escapes, two bytes except at the end of the text: a '\' and any character, or
	//	a CR and the LF which follows it. They draw nothing and start a new line.
	//
	// A '#' which no code follows is plain text, exactly like the official parser
	// treats it.
	namespace textformat
	{
		enum class Unit
		{
			PLAIN,		// Characters up to the next delimiter, drawn as they are
			STYLE,		// Two byte code which switches the colour, the boldness or the select range
			MARKER,		// Two byte code which draws no text (markers, #w, ##)
			PAYLOAD,	// Payload code, hidden, its meaning is up to the caller
			LITERAL,	// '#', a code which does not exist: drawn with the character after it
			BREAK		// Escape or CR, hidden, starts a new line
		};

		// The kind of the unit which starts at [index] and its byte length there
		// (at least one)
		Unit classify(const std::string& text, size_t index, size_t& length);

		// Decode the UTF-8 codepoint which starts at the given index and report how
		// many bytes it took. Invalid sequences decode to U+FFFD and consume one byte,
		// so the decoder always makes progress. All text the client renders is UTF-8:
		// the server encodes strings in its client charset (see InPacket).
		uint32_t utf8_decode(const std::string& text, size_t index, size_t& length);
	}
}
