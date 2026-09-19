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
//	You should have received a copy of the GNU Affero General Public License		//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>

namespace ms
{
	// Where the lines the LOG macro produces go: the console, a rotating file and
	// the buffer the log window reads. The buffer is bounded by the settings
	// LogLines and LogSeconds and drops the oldest lines first, the file rolls over
	// at LogFileMB, so neither grows with the uptime of the client.
	namespace log
	{
		// One line of output, timed by both clocks: steady_ms measures the retention
		// window, wall_ms is what the window and the file print
		struct Entry
		{
			int64_t steady_ms;
			int64_t wall_ms;
			int level;
			std::string text;
		};

		// Name of a level, e.g. "NETWORK" for LOG_NETWORK
		const char* level_name(int level);

		// Write the wall clock time of a line as "HH:MM:SS.mmm"; the buffer needs
		// room for 13 characters
		void format_clock(int64_t wall_ms, char* out, size_t size);

		// Put a line of the given level into the sinks
		void write(int level, const std::string& message);

		// The buffer a line is assembled in; one per thread, so lines logged from
		// several threads do not mix
		std::ostringstream& buffer();

		// Read access to the lines kept in memory, oldest first. The log is locked
		// for as long as the object lives, so a reader draws from a buffer nothing
		// writes to; lines() hands out the buffer itself, no copy is made. The lock
		// is recursive, so a reader which logs while it holds the lock still works.
		class Locked
		{
		public:
			Locked();
			~Locked();

			const std::deque<Entry>& lines() const;
			// How many lines were dropped before the first kept one; the ordinal a
			// line is addressed by is dropped() + its position in lines()
			size_t dropped() const;

		private:
			std::unique_lock<std::recursive_mutex> lock;
		};

		// Drop the lines kept in memory; the file keeps what was written to it
		void clear();

		// The line LOG(level, message) assembles: the message is streamed into the
		// thread buffer and handed to write() when the statement ends. A level above
		// LOG_LEVEL is skipped before anything is assembled.
		class Line
		{
		public:
			Line(int level, bool enabled) : level(level), enabled(enabled)
			{
				if (enabled)
				{
					std::ostringstream& out = buffer();

					out.str(std::string());
					out.clear();
				}
			}

			~Line()
			{
				if (enabled)
					write(level, buffer().str());
			}

			template <typename T>
			Line& operator<<(const T& value)
			{
				if (enabled)
					buffer() << value;

				return *this;
			}

		private:
			int level;
			bool enabled;
		};
	}
}
