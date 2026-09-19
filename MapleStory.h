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

// The sink the LOG macro below feeds (the line builder it expands to)
#include "Util/Log.h"

// If defined use Asio for networking, otherwise use Winsock.
//#define USE_ASIO

// Use cryptography for communication with the server
#define USE_CRYPTO

// If defined use NX, otherwise use WZ.
#define USE_NX

// If defined, the single-package *.nx set of a pre-split client is accepted as well.
// Those clients (v83, and v95 as well) ship one *.wz per category, so an NX conversion of
// them has Base.nx, Character.nx, ..., Map.nx, Mob.nx, Morph.nx, Skill.nx, Sound.nx, UI.nx
// and none of the split files (Map001.nx, Mob001.nx, Skill001.nx, Sound001.nx, ...).
// With this defined the split files are optional and NxFiles::init() points their roots at
// the single package the data actually lives in, which keeps the readers that address the
// roots by name working (Map001 for Back/, Map002 for Map/ and Effect.img, Sound002 for
// music). The UI.nx version test is reported instead of being fatal, because a v83 UI.nx
// predates the layout those screens need. Comment out to require the 28-file split set.
#define USE_NX_V83

// If defined read the loose .img data folder ("<cwd>/data/<Category>/...") instead
// of .nx files. USE_NX stays defined because it also selects the nl:: node API the
// client source is written against; USE_IMG swaps the backend behind it for
// includes/ImgLib.
#define USE_IMG

// Debug options
#define LOG_ERROR	1
#define LOG_WARN	2
#define LOG_INFO	3
#define LOG_DEBUG	4
#define LOG_NETWORK	5
#define LOG_UI		6
#define LOG_TRACE	7

// Log Level
#ifdef _DEBUG
	#define LOG_LEVEL LOG_NETWORK
#else
	#define LOG_LEVEL LOG_WARN
#endif

// Log Commands
// A line is assembled by the sink (Util/Log.h) and written to the console, kept
// in memory for the log window and appended to a rotating file. The level names
// are log::level_name(); a release build compiles the lines out entirely.
#ifdef _DEBUG
	#define LOG(level, message) ms::log::Line(level, (level) <= LOG_LEVEL) << message
#else
	#define LOG(level, message) void(0)
#endif