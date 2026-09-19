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
#include "Window.h"

#include "UI.h"

#include "../Configuration.h"
#include "../Timer.h"
#include "../Util/DebugUI.h"
#include "../Util/ScreenResolution.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <Windows.h>

namespace ms
{
	Window::Window()
	{
		context = nullptr;
		glwnd = nullptr;
		cursorcaptured = false;
		opacity = 1.0f;
		opcstep = 0.0f;
		width = Constants::Constants::get().get_viewwidth();
		height = Constants::Constants::get().get_viewheight();
	}

	Window::~Window()
	{
		glfwTerminate();
	}

	void error_callback(int no, const char* description)
	{
		LOG(LOG_ERROR, "GLFW error [" << no << "]: " << description);
	}

	void key_callback(GLFWwindow*, int key, int, int action, int)
	{
		// A debug window takes the keyboard while it is used: what is typed into it
		// (a filter, a text field) is not a game key
		if (debugui::captures_keyboard())
			return;

		UI::get().send_key(key, action != GLFW_RELEASE, action == GLFW_REPEAT);
	}

	std::chrono::time_point<std::chrono::steady_clock> start = ContinuousTimer::get().start();

	void mousekey_callback(GLFWwindow*, int button, int action, int)
	{
		// A debug window takes the mouse while it is used; the clicks that go into
		// it are not game clicks
		if (debugui::captures_mouse())
			return;

		switch (button)
		{
			case GLFW_MOUSE_BUTTON_LEFT:
			{
				switch (action)
				{
					case GLFW_PRESS:
					{
						UI::get().send_cursor(true);
						break;
					}
					case GLFW_RELEASE:
					{
						auto diff_ms = ContinuousTimer::get().stop(start) / 1000;

						start = ContinuousTimer::get().start();

						if (diff_ms > 10 && diff_ms < 200)
							UI::get().doubleclick();

						UI::get().send_cursor(false);
						break;
					}
				}

				break;
			}
			case GLFW_MOUSE_BUTTON_RIGHT:
			{
				switch (action)
				{
					case GLFW_PRESS:
						UI::get().rightclick();
						break;
				}

				break;
			}
		}
	}

	void cursor_callback(GLFWwindow*, double xpos, double ypos)
	{
		// The cursor belongs to the debug window while it is over one; the game
		// cursor stays where it was
		if (debugui::captures_mouse())
			return;

		Point<int16_t> cursor_position = Point<int16_t>(
			static_cast<int16_t>(xpos),
			static_cast<int16_t>(ypos)
			);

		Point<int16_t> screen = Point<int16_t>(
			Constants::Constants::get().get_viewwidth(),
			Constants::Constants::get().get_viewheight()
			);

		if (cursor_position.x() > 0 && cursor_position.y() > 0)
			if (cursor_position.x() < screen.x() && cursor_position.y() < screen.y())
				UI::get().send_cursor(cursor_position);
	}

	void focus_callback(GLFWwindow*, int focused)
	{
		UI::get().send_focus(focused);
	}

	void scroll_callback(GLFWwindow*, double xoffset, double yoffset)
	{
		if (debugui::captures_mouse())
			return;

		UI::get().send_scroll(yoffset);
	}

	void close_callback(GLFWwindow* window)
	{
		UI::get().send_close();

		glfwSetWindowShouldClose(window, GL_FALSE);
	}

	Error Window::init()
	{
		fullscreen = Setting<Fullscreen>::get().load();

		if (!glfwInit())
			return Error::Code::GLFW;

		// Read the desktop size here and not before glfwInit: glfwInit makes the
		// process DPI aware, before that Windows reports the size with the
		// display scaling applied
		ScreenResolution();

		LOG(LOG_INFO, "Desktop size: " << Configuration::get().get_max_width() << 'x' << Configuration::get().get_max_height());

		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		context = glfwCreateWindow(1, 1, "", nullptr, nullptr);
		glfwMakeContextCurrent(context);
		glfwSetErrorCallback(error_callback);
		glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

		if (Error error = GraphicsGL::get().init())
			return error;

		// The debug windows need no window of their own; their backends are bound to
		// the one the game draws in (initwindow)
		debugui::init();

		return initwindow();
	}

	Error Window::initwindow()
	{
		if (glwnd)
			glfwDestroyWindow(glwnd);

		glwnd = glfwCreateWindow(
			width,
			height,
			Configuration::get().get_title().c_str(),
			fullscreen ? glfwGetPrimaryMonitor() : nullptr,
			context
		);

		if (!glwnd)
			return Error::Code::WINDOW;

		glfwMakeContextCurrent(glwnd);

		bool vsync = Setting<VSync>::get().load();
		glfwSwapInterval(vsync ? 1 : 0);

		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glfwSetInputMode(glwnd, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

		cursorcaptured = false;

		double xpos, ypos;

		glfwGetCursorPos(glwnd, &xpos, &ypos);
		cursor_callback(glwnd, xpos, ypos);

		glfwSetInputMode(glwnd, GLFW_STICKY_KEYS, GL_TRUE);
		glfwSetKeyCallback(glwnd, key_callback);
		glfwSetMouseButtonCallback(glwnd, mousekey_callback);
		glfwSetCursorPosCallback(glwnd, cursor_callback);
		glfwSetWindowFocusCallback(glwnd, focus_callback);
		glfwSetScrollCallback(glwnd, scroll_callback);
		glfwSetWindowCloseCallback(glwnd, close_callback);

		char buf[256];
		GetCurrentDirectoryA(256, buf);
		strcat_s(buf, sizeof(buf), "\\Icon.png");

		GLFWimage images[1];

		auto stbi = stbi_load(buf, &images[0].width, &images[0].height, 0, 4);

		if (stbi == NULL)
			return Error(Error::Code::MISSING_ICON, stbi_failure_reason());

		images[0].pixels = stbi;

		glfwSetWindowIcon(glwnd, 1, images);
		stbi_image_free(images[0].pixels);

		GraphicsGL::get().reinit();

		// The window is created again when the screen mode changes, and the debug
		// backends were bound to the one this replaces
		debugui::attach(glwnd);

		return Error::Code::NONE;
	}

	bool Window::not_closed() const
	{
		return glfwWindowShouldClose(glwnd) == 0;
	}

	void Window::update()
	{
		updateopc();

		// The game draws its own cursor and keeps the system one hidden; while a
		// debug window is used the system one has to be shown, or there would be
		// two of them
		bool captured = debugui::captures_mouse();

		if (captured != cursorcaptured)
		{
			glfwSetInputMode(glwnd, GLFW_CURSOR, captured ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);

			cursorcaptured = captured;
		}
	}

	void Window::updateopc()
	{
		if (opcstep != 0.0f)
		{
			opacity += opcstep;

			if (opacity >= 1.0f)
			{
				opacity = 1.0f;
				opcstep = 0.0f;
			}
			else if (opacity <= 0.0f)
			{
				opacity = 0.0f;
				opcstep = -opcstep;

				fadeprocedure();
			}
		}
	}

	void Window::check_events()
	{
		int16_t max_width = Configuration::get().get_max_width();
		int16_t max_height = Configuration::get().get_max_height();
		int16_t new_width = Constants::Constants::get().get_viewwidth();
		int16_t new_height = Constants::Constants::get().get_viewheight();

		if (width != new_width || height != new_height)
		{
			width = new_width;
			height = new_height;

			if (max_width > 0 && max_height > 0 && (new_width >= max_width || new_height >= max_height))
			{
				fullscreen = true;

				LOG(LOG_INFO, "Full screen: on, " << new_width << 'x' << new_height << " is at or above the desktop size");
			}

			initwindow();
		}

		glfwPollEvents();
	}

	void Window::begin() const
	{
		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);

		// The debug windows are drawn over the game, so they come after its flush
		// and before the buffers are swapped
		debugui::draw();

		glfwSwapBuffers(glwnd);
	}

	void Window::fadeout(float step, std::function<void()> fadeproc)
	{
		opcstep = -step;
		fadeprocedure = fadeproc;
	}

	void Window::setclipboard(const std::string& text) const
	{
		glfwSetClipboardString(glwnd, text.c_str());
	}

	std::string Window::getclipboard() const
	{
		const char* text = glfwGetClipboardString(glwnd);

		return text ? text : "";
	}

	void Window::toggle_fullscreen()
	{
		int16_t max_width = Configuration::get().get_max_width();
		int16_t max_height = Configuration::get().get_max_height();

		// Leaving full screen is always allowed, entering it only while the
		// window is smaller than the desktop
		if (fullscreen || (width < max_width && height < max_height))
		{
			fullscreen = !fullscreen;
			Setting<Fullscreen>::get().save(fullscreen);

			LOG(LOG_INFO, "Full screen: " << (fullscreen ? "on" : "off"));

			initwindow();
			glfwPollEvents();
		}
		else
		{
			LOG(LOG_INFO, "Full screen: not toggled, " << width << 'x' << height << " is not below the desktop size " << max_width << 'x' << max_height);
		}
	}
}