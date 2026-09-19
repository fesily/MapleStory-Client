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
#include "../MapleStory.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace
{
	// The lines kept in memory, the newest at the back; droppedcount says how many
	// were dropped in front of them, so a reader can tell a moved front from new
	// lines, and keptbytes is what keeping them costs
	std::deque<ms::log::Entry> kept;
	std::recursive_mutex keptmutex;
	size_t keptbytes = 0;
	size_t droppedcount = 0;

	// The buffer stops at this size even when LogLines and LogSeconds would allow
	// more: one line can be long (a packet dump, a whole dialog) and the window
	// only ever shows the tail of what is kept
	const size_t MAX_BYTES = 8 * 1024 * 1024;

	// The file sink: log/client.log rolls over to client.1.log and that one to
	// client.2.log, so the set holds the last LOGFILES * LogFileMB on the disk
	const char* LOGDIRECTORY = "log";
	const char* LOGFILE = "log/client.log";
	const size_t LOGFILES = 3;

	std::ofstream file;
	size_t filesize = 0;

	// The settings the sink keeps its lines by
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

	int64_t wall_ms()
	{
		using clock = std::chrono::system_clock;

		return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
	}

	void make_directory()
	{
#ifdef _WIN32
		_mkdir(LOGDIRECTORY);
#else
		mkdir(LOGDIRECTORY, 0755);
#endif
	}

	std::string rolled_name(size_t index)
	{
		return std::string("log/client.") + std::to_string(index) + ".log";
	}

	void open_file()
	{
		make_directory();

		file.open(LOGFILE, std::ios::binary | std::ios::app);

		filesize = 0;

		// A file left over from an earlier run keeps the size it grew to, so the
		// rollover still happens at the point the setting names
		if (file.is_open())
		{
			file.seekp(0, std::ios::end);

			filesize = static_cast<size_t>(file.tellp());
		}
	}

	// Make the next file the current one and drop the oldest of the set
	void roll_file()
	{
		file.close();

		std::remove(rolled_name(LOGFILES - 1).c_str());

		for (size_t index = LOGFILES - 2; index > 0; index--)
			std::rename(rolled_name(index).c_str(), rolled_name(index + 1).c_str());

		std::rename(LOGFILE, rolled_name(1).c_str());

		open_file();
	}

	void write_file(int level, int64_t stamp, const std::string& message)
	{
		const Retention& kept_by = retention();

		if (!kept_by.tofile || kept_by.filebytes == 0)
			return;

		if (!file.is_open())
			open_file();

		if (!file.is_open())
			return;

		if (filesize + message.size() >= kept_by.filebytes)
			roll_file();

		char clock[13];

		ms::log::format_clock(stamp, clock, sizeof(clock));

		std::string line = std::string("[") + clock + "] [" + ms::log::level_name(level) + "] " + message + '\n';

		file.write(line.data(), static_cast<std::streamsize>(line.size()));
		// Flushed per line, so the lines leading up to a crash are on the disk
		// already; the client writes few enough of them for the cost not to matter
		file.flush();

		filesize += line.size();
	}
}

namespace ms
{
	namespace log
	{
		const char* level_name(int level)
		{
			switch (level)
			{
				case LOG_ERROR:
					return "ERROR";
				case LOG_WARN:
					return "WARN";
				case LOG_INFO:
					return "INFO";
				case LOG_DEBUG:
					return "DEBUG";
				case LOG_NETWORK:
					return "NETWORK";
				case LOG_UI:
					return "UI";
				case LOG_TRACE:
					return "TRACE";
			}

			return "UNDEFINED";
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

		std::ostringstream& buffer()
		{
			// One buffer per thread: the read thread of the network and the game
			// thread both log, and they would otherwise assemble into each other
			static thread_local std::ostringstream out;

			return out;
		}

		void write(int level, const std::string& message)
		{
			const Retention& kept_by = retention();

			std::cout << "[" << level_name(level) << "]: " << message << std::endl;

			int64_t stamp = wall_ms();
			int64_t now = steady_ms();

			std::lock_guard<std::recursive_mutex> lock(keptmutex);

			write_file(level, stamp, message);

			Entry entry;

			entry.steady_ms = now;
			entry.wall_ms = stamp;
			entry.level = level;
			entry.text = message;

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
