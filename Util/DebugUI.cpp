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
	// Set while the window the ImGui state was built for is gone: the next attach
	// builds the state again, because the backends cannot be moved to another window
	bool contextstale = false;
	// Set while the sizes of the style are laid out at the scale of a monitor
	bool stylescaled = false;
	// The scale the debug windows are drawn at: the one the desktop reports and the
	// one the settings ask for
	float desktopscale = 1.0f;
	float userscale = 1.0f;

	// Take the backends down; the window they are bound to has to be alive for it
	void unbind()
	{
		if (!bound)
			return;

		ImGui_ImplGlfw_Shutdown();
		ImGui_ImplOpenGL2_Shutdown();

		bound = false;
	}

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
		namespace
		{
			// Put the ImGui state in place: the flags the backend has to answer, the ini
			// file the windows remember themselves in, the font they print with and the
			// style. It is set up for the first window and again when the window the
			// game draws in is created anew.
			void create_context()
			{
				IMGUI_CHECKVERSION();
				ImGui::CreateContext();

				ImGuiIO& io = ImGui::GetIO();

				io.IniFilename = "debugui.ini";
				io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

				// The debug windows follow the scale the desktop runs at, like the rest
				// of the windows on it: ImGui keeps the font at the scale of the monitor
				// a window is on (and re-rasterizes it there), and sizes the platform
				// windows of the ones which were dragged out of the game window at it
				io.ConfigDpiScaleFonts = true;
				io.ConfigDpiScaleViewports = true;

				const std::string font = Setting<FontPathCJKNormal>::get().load();

				if (!font.empty() && font_available(font))
					io.Fonts->AddFontFromFileTTF(font.c_str(), 15.0f);

				// A rounded corner would show the desktop through the gap it leaves at
				// each of the windows which are drawn outside the game window
				ImGuiStyle& style = ImGui::GetStyle();

				style.WindowRounding = 0.0f;
				style.Colors[ImGuiCol_WindowBg].w = 0.94f;

				// The style of a fresh context holds the sizes the scale is laid over
				stylescaled = false;
			}
		}

		void init()
		{
			create_context();

			ready = true;
		}

		void attach(GLFWwindow* window)
		{
			if (!ready)
				return;

			// The backend subclasses the window it is bound to and holds the platform
			// windows it created for the windows that were dragged out of it; none of
			// that can be moved to the window this replaces. The ImGui state is built
			// again instead, which drops the platform windows of the one that is gone
			// and leaves the new window with the ones it can have.
			bool replacing = contextstale;

			unbind();

			if (replacing)
			{
				ImGui::DestroyContext();

				create_context();
			}

			contextstale = false;

			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplGlfw_SetCallbacksChainForAllWindows(false);
			ImGui_ImplOpenGL2_Init();

			// The font is kept at the scale of the monitor the window is on by ImGui;
			// the sizes and the padding of the widgets are laid out at that scale times
			// what the settings ask for, once per style
			if (!stylescaled)
			{
				float xscale = 1.0f;
				float yscale = 1.0f;

				glfwGetWindowContentScale(window, &xscale, &yscale);

				if (xscale > 0.0f)
				{
					uint16_t percent = Setting<DebugUIScale>::get().load();
					float asked = percent > 0 ? static_cast<float>(percent) / 100.0f : 1.0f;

					ImGuiStyle& style = ImGui::GetStyle();

					style.ScaleAllSizes(xscale * asked);

					// The part of the scale the setting asks for; the part the desktop
					// reports is kept by ImGui in FontScaleDpi, which it overwrites from
					// the monitor of the viewport as soon as one is drawn
					style.FontScaleMain = asked;
					style.FontScaleDpi = xscale;

					userscale = asked;
					desktopscale = xscale;

					stylescaled = true;

					LOG(LOG_INFO, "Debug windows: scale " << (xscale * asked)
						<< " (desktop " << xscale << ", setting " << percent << "%)");
				}
			}

			bound = true;
		}

		void detach()
		{
			// The window the state was built for is going away, which the backends
			// cannot be moved off: the next attach builds the state again
			unbind();

			contextstale = true;
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

			// The list of monitors the backend just filled is checked by the frame that
			// follows, and it stops the client when a scale in it is not a scale: the
			// backend skips a monitor which reports exactly none, but a remote or
			// virtual display can report a negative one or a value with no meaning as
			// well. The list is ours, so such a scale is repaired here.
			for (ImGuiPlatformMonitor& monitor : ImGui::GetPlatformIO().Monitors)
			{
				if (!(monitor.DpiScale > 0.0f && monitor.DpiScale < 99.0f))
				{
					LOG(LOG_WARN, "Debug windows: a monitor reports a scale of " << monitor.DpiScale << ", the windows are drawn at 1");

					monitor.DpiScale = 1.0f;
				}
			}

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

		float scale()
		{
			return desktopscale * userscale;
		}
	}
}
