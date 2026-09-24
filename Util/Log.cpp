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
#include "Log.h"

#include "../Configuration.h"

#include <spdlog/details/os.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <vector>

namespace
{
	using ms::log::Channel;
	using ms::log::Entry;

	// The lines kept in memory, the newest at the back; droppedcount says how many
	// were dropped in front of them, so a reader can tell a moved front from new
	// lines, and keptbytes is what keeping them costs
	std::deque<Entry> kept;
	std::recursive_mutex keptmutex;
	size_t keptbytes = 0;
	size_t droppedcount = 0;

	// The buffer stops at this size even when LogLines and LogSeconds would allow
	// more: one line can be long (a packet dump, a whole dialog) and the window
	// only ever shows the tail of what is kept
	const size_t MAX_BYTES = 8 * 1024 * 1024;

	// The file sink: log/client.log rolls over to client.1.log and that one to
	// client.2.log, so the set holds the last LOGFILES * LogFileMB on the disk.
	// spdlog keeps the file it writes plus ROTATEDFILES rolled over ones.
	const char* LOGDIRECTORY = "log";
	const char* LOGFILE = "log/client.log";
	const size_t LOGFILES = 3;
	const size_t ROTATEDFILES = LOGFILES - 1;

	// The console prints the tag and the message, the file the time in front of it,
	// as both did before the sinks were spdlog's
	const char* CONSOLEPATTERN = "[%l]: %v";
	const char* FILEPATTERN = "[%H:%M:%S.%e] [%l] %v";

	// Every sink ends its lines with a line feed, whatever spdlog defaults to on
	// the platform it runs on
	const char* LINEENDING = "\n";

	// The settings the sinks keep their lines by
	struct Retention
	{
		int64_t seconds;
		size_t lines;
		bool tofile;
		size_t filebytes;
	};

	// The settings are read once: lines are written often enough that reading them
	// again for every one of them would show up in a profile
	const Retention& retention()
	{
		static const Retention kept_by = {
			ms::Setting<ms::LogSeconds>().get().load(),
			ms::Setting<ms::LogLines>().get().load(),
			ms::Setting<ms::LogFile>().get().load(),
			static_cast<size_t>(ms::Setting<ms::LogFileMB>().get().load()) * 1024 * 1024
		};

		return kept_by;
	}

	int64_t steady_ms()
	{
		using clock = std::chrono::steady_clock;

		return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
	}

	// The logger a line went through: spdlog names it with the name the logger was
	// built with, which is the channel name for the two loggers that are not the
	// client's
	Channel channel_of(const spdlog::string_view_t& name)
	{
		if (name == ms::log::channel_name(Channel::NETWORK))
			return Channel::NETWORK;

		if (name == ms::log::channel_name(Channel::UI))
			return Channel::UI;

		return Channel::CLIENT;
	}

	// The tag a line is printed under. spdlog's own %l says "warning" where the
	// client says "WARN" and knows no name for a line the network or the ui
	// logger wrote, and the sinks of the client print what they printed before
	// spdlog replaced them.
	class TagFlag : public spdlog::custom_flag_formatter
	{
	public:
		void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
		{
			const char* tag = ms::log::line_tag(static_cast<int>(msg.level), channel_of(msg.logger_name));

			dest.append(tag, tag + std::strlen(tag));
		}

		std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
		{
			return spdlog::details::make_unique<TagFlag>();
		}
	};

	// The formatter compiles its pattern with the flags it is built with, so the
	// tag flag has to be handed to the constructor: adding it afterwards would
	// leave the pattern built with spdlog's own %l, which prints "warning" where
	// the client says "WARN"
	std::unique_ptr<spdlog::formatter> make_formatter(const char* pattern)
	{
		spdlog::pattern_formatter::custom_flags tags;

		tags['l'] = std::make_unique<TagFlag>();

		return std::make_unique<spdlog::pattern_formatter>(
			std::string(pattern),
			spdlog::pattern_time_type::local,
			std::string(LINEENDING),
			std::move(tags)
		);
	}

	// One line as the window keeps it: the time and the tag are added when it is
	// drawn, so the lines are kept the way they were written
	void keep(const spdlog::details::log_msg& msg)
	{
		const Retention& kept_by = retention();

		int64_t stamp = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()).count();
		int64_t now = steady_ms();

		std::lock_guard<std::recursive_mutex> lock(keptmutex);

		Entry entry;

		entry.steady_ms = now;
		entry.wall_ms = stamp;
		entry.level = static_cast<int>(msg.level);
		entry.channel = channel_of(msg.logger_name);
		entry.text.assign(msg.payload.data(), msg.payload.size());

		keptbytes += entry.text.size();

		kept.push_back(std::move(entry));

		// Drop what is older than the window and what is past the caps, oldest
		// first, so both the memory and the time the buffer covers stay bounded
		int64_t oldest = now - kept_by.seconds * 1000;

		while (!kept.empty() && (kept.size() > kept_by.lines || keptbytes > MAX_BYTES || kept.front().steady_ms < oldest))
		{
			keptbytes -= kept.front().text.size();

			kept.pop_front();
			droppedcount++;
		}
	}

	// The sink the log window reads: the lines are kept in the buffer above
	// instead of being formatted, and the window draws them from there
	class WindowSink : public spdlog::sinks::sink
	{
	public:
		void log(const spdlog::details::log_msg& msg) override
		{
			keep(msg);
		}

		void flush() override {}
		void set_pattern(const std::string&) override {}
		void set_formatter(std::unique_ptr<spdlog::formatter>) override {}
	};

	// The level the loggers run at: the setting, or the level the client always
	// showed when the setting names no level
	spdlog::level::level_enum configured_level()
	{
		spdlog::level::level_enum level;

		if (!ms::log::parse_level(ms::Setting<ms::LogLevel>().get().load(), level))
			level = spdlog::level::debug;

		return level;
	}

	std::vector<spdlog::sink_ptr> make_sinks()
	{
		const Retention& kept_by = retention();

		std::vector<spdlog::sink_ptr> sinks;

		// The console sink keeps no handle of its own, so it writes nothing in a
		// build that has no console attached
		sinks.push_back(std::make_shared<spdlog::sinks::stderr_sink_mt>());
		sinks.back()->set_formatter(make_formatter(CONSOLEPATTERN));

		if (kept_by.tofile && kept_by.filebytes > 0)
		{
			spdlog::details::os::create_dir(SPDLOG_FILENAME_T(LOGDIRECTORY));

			sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
				LOGFILE,
				kept_by.filebytes,
				ROTATEDFILES
			));

			sinks.back()->set_formatter(make_formatter(FILEPATTERN));
		}

		sinks.push_back(std::make_shared<WindowSink>());

		return sinks;
	}

	// Whether the loggers are up: they are built with the first line that is
	// logged, so a session that logs nothing opens nothing either
	bool started = false;

	// The level the loggers are built with: the one init() read from the settings,
	// or the one the settings name when the first line comes before that
	bool levelread = false;
	spdlog::level::level_enum readlevel = spdlog::level::debug;

	spdlog::level::level_enum pending_level()
	{
		return levelread ? readlevel : configured_level();
	}

	std::array<std::shared_ptr<spdlog::logger>, static_cast<size_t>(Channel::COUNT)>& loggers()
	{
		// One set of sinks for the three of them, so a line is written once and the
		// three see it in the same order
		static std::array<std::shared_ptr<spdlog::logger>, static_cast<size_t>(Channel::COUNT)> built = [] {
			std::vector<spdlog::sink_ptr> sinks = make_sinks();
			std::array<std::shared_ptr<spdlog::logger>, static_cast<size_t>(Channel::COUNT)> loggers;

			for (size_t index = 0; index < loggers.size(); index++)
			{
				loggers[index] = std::make_shared<spdlog::logger>(
					ms::log::channel_name(static_cast<Channel>(index)),
					sinks.begin(),
					sinks.end()
				);

				// An error and everything above it is on the disk as soon as it is
				// written; the rest follows on the flush of the frame loop
				loggers[index]->flush_on(spdlog::level::err);
				loggers[index]->set_level(pending_level());
			}

			started = true;

			return loggers;
		}();

		return built;
	}
}

namespace ms
{
	namespace log
	{
		const char* severity_name(int severity)
		{
			switch (severity)
			{
				case spdlog::level::err:
					return "ERROR";
				case spdlog::level::warn:
					return "WARN";
				case spdlog::level::info:
					return "INFO";
				case spdlog::level::debug:
					return "DEBUG";
				case spdlog::level::trace:
					return "TRACE";
			}

			return "UNDEFINED";
		}

		const char* channel_name(Channel channel)
		{
			switch (channel)
			{
				case Channel::CLIENT:
					return "CLIENT";
				case Channel::NETWORK:
					return "NETWORK";
				case Channel::UI:
					return "UI";
			}

			return "UNDEFINED";
		}

		const char* line_tag(int severity, Channel channel)
		{
			return channel == Channel::CLIENT ? severity_name(severity) : channel_name(channel);
		}

		spdlog::logger& logger(Channel channel)
		{
			return *loggers()[static_cast<size_t>(channel)];
		}

		void init()
		{
			// The settings are read here, where they are up; the loggers take the
			// level when the first line is logged, which is also when the file is
			// opened, so a session that logs nothing opens nothing
			readlevel = configured_level();
			levelread = true;
		}

		void flush()
		{
			// Nothing was logged, so nothing is open
			if (!started)
				return;

			// The three loggers share their sinks, so one of them flushes all of them
			logger(Channel::CLIENT).flush();
		}

		void set_level(spdlog::level::level_enum level)
		{
			readlevel = level;
			levelread = true;

			// Before the first line there are no loggers to put it on
			if (!started)
				return;

			for (size_t index = 0; index < static_cast<size_t>(Channel::COUNT); index++)
				logger(static_cast<Channel>(index)).set_level(level);
		}

		bool parse_level(const std::string& name, spdlog::level::level_enum& level)
		{
			if (name.empty())
				return false;

			level = spdlog::level::from_str(name);

			// from_str answers "off" for a name it does not know, so a real "off"
			// has to be told apart from a typo
			return level != spdlog::level::off || name == "off";
		}

		void format_clock(int64_t stamp, char* out, size_t size)
		{
			std::time_t seconds = static_cast<std::time_t>(stamp / 1000);
			std::tm time = {};

#ifdef _WIN32
			localtime_s(&time, &seconds);
#else
			std::tm* local = std::localtime(&seconds);

			if (local)
				time = *local;
#endif

			std::snprintf(
				out,
				size,
				"%02d:%02d:%02d.%03d",
				time.tm_hour,
				time.tm_min,
				time.tm_sec,
				static_cast<int>(stamp % 1000)
			);
		}

		void clear()
		{
			std::lock_guard<std::recursive_mutex> lock(keptmutex);

			kept.clear();

			keptbytes = 0;
			droppedcount = 0;
		}

		Locked::Locked() : lock(keptmutex) {}

		Locked::~Locked() {}

		const std::deque<Entry>& Locked::lines() const
		{
			return kept;
		}

		size_t Locked::dropped() const
		{
			return droppedcount;
		}
	}
}
