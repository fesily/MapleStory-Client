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
#include "DebugUI.h"

// The backends draw with the fixed pipeline and share the context state with the
// renderer, which loads OpenGL through glew
#define GLEW_STATIC
#include <glew.h>
#include <glfw3.h>

#include "CommandWindow.h"
#include "LogWindow.h"

#include "../Configuration.h"
#include "../Graphics/GraphicsGL.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"

#include <cstdio>
#include <string>

namespace
{
	bool ready = false;
	bool bound = false;

	// Whether the font file is there; ImGui keeps its own font when the client's is
	// missing, and that one has no CJK, which the log needs (names, chat, dialogs)
	bool font_available(const std::string& path)
	{
		FILE* file = nullptr;

#ifdef _WIN32
		if (fopen_s(&file, path.c_str(), "rb") != 0)
			file = nullptr;
#else
		file = std::fopen(path.c_str(), "rb");
#endif

		if (!file)
			return false;

		std::fclose(file);

		return true;
	}
}

namespace ms
{
	namespace debugui
	{
		void init()
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO();

			// The windows remember where they were put; a window dragged out of the
			// game window is one of its own and stays there
			io.IniFilename = "debugui.ini";
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

			const std::string font = Setting<FontPathCJKNormal>().get().load();

			if (!font.empty() && font_available(font))
				io.Fonts->AddFontFromFileTTF(font.c_str(), 15.0f);

			// A rounded corner would show the desktop through the gap it leaves at
			// each of the windows which are drawn outside the game window
			ImGuiStyle& style = ImGui::GetStyle();

			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 0.94f;

			ready = true;
		}

		void attach(GLFWwindow* window)
		{
			if (!ready)
				return;

			// The window the game draws in is created again when the screen mode
			// changes; the backends remember the window they were bound to, so they
			// are taken down and set up for the new one. Their callbacks chain to the
			// game callbacks that were installed before them.
			if (bound)
			{
				ImGui_ImplGlfw_Shutdown();
				ImGui_ImplOpenGL2_Shutdown();
			}

			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplGlfw_SetCallbacksChainForAllWindows(false);
			ImGui_ImplOpenGL2_Init();

			bound = true;
		}

		void draw()
		{
			if (!ready)
				return;

			// The renderer keeps its shader program bound between frames, and the
			// backend draws with the fixed pipeline: with that program still bound
			// the overlay would be drawn by its vertex shader
			glUseProgram(0);

			ImGui_ImplOpenGL2_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			log_window::draw();
			console_window::draw();

			ImGui::Render();

			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				// A window dragged out of the game window is drawn into a window of
				// its own; the backend switches the context for it and the game's own
				// is made current again
				GLFWwindow* backup = glfwGetCurrentContext();

				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();

				glfwMakeContextCurrent(backup);
			}

			// The backend draws with client arrays where the renderer left its own
			// vertex buffer, and the viewport of a window drawn into is not the one
			// the game draws with either
			GraphicsGL::get().resetstate();
		}

		bool captures_mouse()
		{
			return ready && ImGui::GetIO().WantCaptureMouse;
		}

		bool captures_keyboard()
		{
			return ready && ImGui::GetIO().WantCaptureKeyboard;
		}

		void set_log_visible(bool visible)
		{
			log_window::set_visible(visible);
		}

		void toggle_log()
		{
			log_window::set_visible(!log_window::visible());
		}

		void set_console_visible(bool visible)
		{
			console_window::set_visible(visible);
		}
	}
}
