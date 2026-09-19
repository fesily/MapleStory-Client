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
#include "Gameplay/Stage.h"
#include "Gameplay/MapleMap/MapObjects.h"
#include "Gameplay/MapleMap/Npc.h"
#include "IO/UI.h"
#include "IO/LoginScript.h"
#include "IO/UiScript.h"
#include "IO/UITypes/UINpcTalk.h"
#include "IO/Window.h"
#include "Net/Packets/GameplayPackets.h"
#include "Net/Packets/MessagingPackets.h"
#include "Net/Packets/NpcInteractionPackets.h"
#include "Net/Session.h"
#include "Util/DebugConsole.h"
#include "Util/DebugUI.h"
#include "Util/HardwareInfo.h"
#include "Util/Log.h"
#include "Util/Misc.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(USE_IMG)
#include "Util/ImgFiles.h"
#elif defined(USE_NX)
#include "Util/NxFiles.h"
#else
#include "Util/WzFiles.h"
#endif

namespace ms
{
	namespace
	{
		// ---- commands of the debug console ----

		// The NPCs the server spawned on this map
		MapObjects& map_npcs()
		{
			return *Stage::get().get_npcs().get_npcs();
		}

		int32_t distance_to_player(const Npc& npc)
		{
			Point<int16_t> diff = npc.get_position() - Stage::get().get_player().get_position();

			return std::abs(static_cast<int32_t>(diff.x())) + std::abs(static_cast<int32_t>(diff.y()));
		}

		void command_npcs(const std::string&)
		{
			size_t count = 0;

			for (auto& entry : map_npcs())
			{
				Npc* npc = static_cast<Npc*>(entry.second.get());

				if (!npc)
					continue;

				// 'script' tells the NPCs the server would open a script for apart
				// from the ones which only have a tag to show
				std::cout << "oid=" << entry.first << " id=" << npc->get_npcid()
					<< " name=\"" << npc->get_name() << "\""
					<< " script=" << (npc->isscripted() ? "yes" : "no")
					<< " dist=" << distance_to_player(*npc) << std::endl;

				count++;
			}

			if (count == 0)
				std::cout << "npcs: no NPC on this map" << std::endl;
		}

		// The NPC the argument refers to: the object id first, then the nearest of the
		// NPCs which carry the id as their template id
		Npc* find_npc(int32_t id)
		{
			Npc* nearest = nullptr;
			int32_t nearestdistance = 0;

			for (auto& entry : map_npcs())
			{
				Npc* npc = static_cast<Npc*>(entry.second.get());

				if (!npc)
					continue;

				if (entry.first == id)
					return npc;

				if (npc->get_npcid() != id)
					continue;

				int32_t distance = distance_to_player(*npc);

				if (!nearest || distance < nearestdistance)
				{
					nearest = npc;
					nearestdistance = distance;
				}
			}

			return nearest;
		}

		void command_talk(const std::string& args)
		{
			if (args.empty())
			{
				std::cout << "Usage: talk <npcid|oid>" << std::endl;
				return;
			}

			int32_t id = string_conversion::or_default<int32_t>(args, 0);

			if (id == 0)
			{
				std::cout << "talk: not a number: " << args << std::endl;
				return;
			}

			Npc* npc = find_npc(id);

			if (!npc)
			{
				// The server looks the object id up on the map the player is in
				// (NPCTalkHandler), so this request is dropped unless the NPC is
				// spawned here; send it anyway and tell what happened
				std::cout << "talk: no NPC " << id << " on this map, sending it as an object id" << std::endl;

				TalkToNPCPacket(id).dispatch();

				return;
			}

			std::cout << "talk: TALK_TO_NPC oid=" << npc->get_oid()
				<< " id=" << npc->get_npcid()
				<< " name=\"" << npc->get_name() << "\""
				<< " dist=" << distance_to_player(*npc) << std::endl;

			TalkToNPCPacket(npc->get_oid()).dispatch();
		}

		void command_center(const std::string&)
		{
			// The server reads no arguments and answers with the script of NPC
			// 9900001 (EnterMTSHandler.openCenterScript)
			EnterMTSPacket().dispatch();

			std::cout << "center: ENTER_MTS sent" << std::endl;
		}

		void command_chat(const std::string& args)
		{
			if (args.empty())
			{
				std::cout << "Usage: chat <text>" << std::endl;
				return;
			}

			// The line the chat bar sends; a line the server reads as a command
			// starts with '!' ('@' for the ones every player can use)
			GeneralChatPacket(args, true).dispatch();

			std::cout << "chat: " << args << std::endl;
		}

		void command_npctalk(const std::string& args)
		{
			if (args.empty())
			{
				std::cout << "Usage: npctalk <file|text>" << std::endl;
				return;
			}

			// The text of a dialog is read from a file when the argument names one, so
			// a dialog can be looked at without rebuilding the client
			std::string text = args;
			std::ifstream file(args, std::ios::binary);
			bool fromfile = file.is_open();

			if (fromfile)
			{
				std::stringstream buffer;
				buffer << file.rdbuf();

				text = buffer.str();
			}

			NpcTalkDialogue dialogue;
			dialogue.npcid = 9010000;
			// The entries of the dialog are part of its text (#L<id>#<label>#l), which
			// is what the server's center script sends
			dialogue.msgtype = 4;
			dialogue.speaker = 0;
			dialogue.text = text;

			UI::get().emplace<UINpcTalk>();
			UI::get().enable();

			if (auto npctalk = UI::get().get_element<UINpcTalk>())
				npctalk->change_text(dialogue);
			else
				std::cout << "npctalk: no game state to show the dialog in" << std::endl;

			std::cout << "npctalk: " << text.size() << " bytes"
				<< (fromfile ? " from " + args : std::string()) << std::endl;
		}

		void command_quit(const std::string&)
		{
			std::cout << "Closing the client." << std::endl;

			UI::get().quit();
		}

		void command_log(const std::string& args)
		{
			if (args == "clear")
			{
				log::clear();

				std::cout << "log: the lines kept in memory are dropped" << std::endl;
			}
			else if (args == "off")
			{
				debugui::set_log_visible(false);

				std::cout << "log: window hidden, 'log on' shows it again" << std::endl;
			}
			else if (args == "on" || args.empty())
			{
				debugui::set_log_visible(true);

				std::cout << "log: window shown" << std::endl;
			}
			else
			{
				std::cout << "Usage: log [on|off|clear]" << std::endl;
			}
		}

		void register_commands()
		{
			debug_console::add({
				{ "center", "", "open the server's center UI (NPC 9900001)", command_center },
				{ "chat", "<text>", "send a chat line, '!' starts a server command", command_chat },
				{ "log", "[on|off|clear]", "show or hide the log window, or clear the lines it keeps", command_log },
				{ "npcs", "", "list the NPCs on this map", command_npcs },
				{ "npctalk", "<file|text>", "show an NPC dialog of a local text", command_npctalk },
				{ "quit", "", "close the client", command_quit },
				{ "talk", "<npcid|oid>", "ask the server for that NPC's dialog", command_talk },
			});

			login_script::register_commands();
			ui_script::register_commands();
		}
	}

	Error init()
	{
		if (Error error = Session::get().init())
			return error;

#if defined(USE_IMG)
		if (Error error = ImgFiles::init())
			return error;
#elif defined(USE_NX)
		if (Error error = NxFiles::init())
			return error;
#else
		if (Error error = WzFiles::init())
			return error;
#endif

		if (Error error = Window::get().init())
			return error;

		if (Error error = Sound::init())
			return error;

		if (Error error = Music::init())
			return error;

		Char::init();
		DamageNumber::init();
		MapPortals::init();
		Stage::get().init();
		UI::get().init();

		return Error::NONE;
	}

	void update()
	{
		Window::get().check_events();
		Window::get().update();
		Stage::get().update();
		UI::get().update();
		Session::get().read();
	}

	void draw(float alpha)
	{
		Window::get().begin();
		Stage::get().draw(alpha);
		UI::get().draw(alpha);
		Window::get().end();
	}

	bool running()
	{
		return Session::get().is_connected()
			&& UI::get().not_quitted()
			&& Window::get().not_closed();
	}

	void loop()
	{
		Timer::get().start();

		int64_t timestep = Constants::TIMESTEP * 1000;
		int64_t accumulator = timestep;

		int64_t period = 0;
		int32_t samples = 0;

		bool show_fps = Configuration::get().get_show_fps();

		while (running())
		{
			debug_console::poll();
			login_script::tick();

			int64_t elapsed = Timer::get().stop();

			// Update game with constant timestep as many times as possible.
			for (accumulator += elapsed; accumulator >= timestep; accumulator -= timestep)
				update();

			// Draw the game. Interpolate to account for remaining time.
			float alpha = static_cast<float>(accumulator) / timestep;
			draw(alpha);

			if (show_fps)
			{
				if (samples < 100)
				{
					period += elapsed;
					samples++;
				}
				else if (period)
				{
					int64_t fps = (samples * 1000000) / period;

					LOG(LOG_INFO, "FPS: " << fps);

					period = 0;
					samples = 0;
				}
			}
		}

		Sound::close();
	}

	void start()
	{
		// Initialize and check for errors
		if (Error error = init())
		{
			const char* message = error.get_message();
			const char* args = error.get_args();
			bool can_retry = error.can_retry();

			if (args && args[0])
				LOG(LOG_ERROR, message << args);
			else
				LOG(LOG_ERROR, message);

			if (can_retry)
				LOG(LOG_INFO, "Enter 'retry' to try again.");

			std::string command;
			std::cin >> command;

			if (can_retry && command == "retry")
				start();
		}
		else
		{
			debug_console::start();
			register_commands();
			loop();
		}
	}
}

#ifdef _DEBUG
int main()
#else
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
#endif
{
	ms::HardwareInfo();
	ms::start();

	return 0;
}