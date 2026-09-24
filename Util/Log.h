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

// The sink side of the log is spdlog (includes/spdlog, upstream v1.17.0, built as
// the library its own CMake target builds - see SPDLOG_COMPILED_LIB in the
// project). What the client adds on top is the channel a line was written from:
// the network and the ui are loggers of their own and not levels, so a packet
// dump is a debug line of the network logger while a client line keeps spdlog's
// own severity. The lines go to the console, a rotating file and the buffer the
// log window reads; the two are formatted the way they were before the sinks
// were spdlog's, and a tag is printed by a flag of our own because spdlog calls
// a warning "warning" where the client calls it "WARN".
#include <spdlog/logger.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ms
{
	namespace log
	{
		// The logger a line goes through; the name of it is what a line that the
		// network or the ui wrote is tagged with
		enum class Channel : int
		{
			CLIENT = 0,
			NETWORK,
			UI,
			COUNT
		};

		// One line's channel and severity. The constants below name both, so a call
		// site still passes a single token: LOG_NETWORK is a debug line of the
		// network logger, LOG_ERROR an error of the client logger.
		struct Level
		{
			Channel channel;
			spdlog::level::level_enum severity;
		};

		// Name of a severity, e.g. "ERROR" for a line of the level spdlog calls
		// err, and of a channel, e.g. "NETWORK". What a line is tagged with is
		// line_tag: the channel for the lines the network and the ui wrote, the
		// severity for a client line.
		const char* severity_name(int severity);
		const char* channel_name(Channel channel);
		const char* line_tag(int severity, Channel channel);

		// The logger of a channel. The three of them share their sinks and are
		// built with the settings when the first line is logged, or by init().
		spdlog::logger& logger(Channel channel);

		// The line LOG() hands to the logger of its channel. The format string has
		// to be a literal: it is checked against the arguments while the client is
		// compiled, in the consteval constructor fmt gives fmt::format_string from
		// C++20 on, which is why the arguments are passed on unchanged.
		template <typename... Args>
		void write(const Level& level, fmt::format_string<Args...> fmt, Args&&... args)
		{
			logger(level.channel).log(level.severity, fmt, std::forward<Args>(args)...);
		}

		// Read the settings and put the loggers on the level they name. A line
		// logged before this goes through the loggers built with the defaults.
		void init();

		// Push what the sinks hold to their files. The client calls this every so
		// many frames instead of flushing after every line.
		void flush();

		// Put the loggers on a level while the client runs, e.g. from the console's
		// 'log level' command
		void set_level(spdlog::level::level_enum level);

		// Read a level name ("trace", "debug", "info", "warn", "error", "off");
		// false when it names none
		bool parse_level(const std::string& name, spdlog::level::level_enum& level);

		// One line of output, timed by both clocks: steady_ms measures the retention
		// window, wall_ms is what the window and the file print
		struct Entry
		{
			int64_t steady_ms;
			int64_t wall_ms;
			int level;
			Channel channel;
			std::string text;
		};

		// Write the wall clock time of a line as "HH:MM:SS.mmm"; the buffer needs
		// room for 13 characters
		void format_clock(int64_t wall_ms, char* out, size_t size);

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
	}
}

// The lines of a debug build go through the logger of their channel; a release
// build compiles them out entirely, so nothing of a call is evaluated there.
// Everything after the level is handed over as it is: the format string and its
// arguments reach write() unchanged, so a call without arguments needs nothing
// special and the macro reads the same under either preprocessor.
#ifdef _DEBUG
	#define LOG(level, ...) ms::log::write((level), __VA_ARGS__)
#else
	#define LOG(level, ...) void(0)
#endif

// Scaffolding that costs work per frame or per packet, so it is compiled out
// unless it is switched on here. Whether a line reaches the sinks is not decided
// by this, the level the loggers run at is.
#define LOG_UI_DRAW 0
#define LOG_TRACE_STATS 0

#ifdef _DEBUG
	#define LOG_PACKET_TRACE 1
#else
	#define LOG_PACKET_TRACE 0
#endif

// The token a call site passes is a channel and a severity at once
constexpr ms::log::Level LOG_ERROR   { ms::log::Channel::CLIENT,  spdlog::level::err   };
constexpr ms::log::Level LOG_WARN    { ms::log::Channel::CLIENT,  spdlog::level::warn  };
constexpr ms::log::Level LOG_INFO    { ms::log::Channel::CLIENT,  spdlog::level::info  };
constexpr ms::log::Level LOG_DEBUG   { ms::log::Channel::CLIENT,  spdlog::level::debug };
constexpr ms::log::Level LOG_TRACE   { ms::log::Channel::CLIENT,  spdlog::level::trace };
constexpr ms::log::Level LOG_NETWORK { ms::log::Channel::NETWORK, spdlog::level::debug };
constexpr ms::log::Level LOG_UI      { ms::log::Channel::UI,      spdlog::level::debug };
