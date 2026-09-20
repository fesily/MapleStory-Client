# HeavenClient

HeavenClient is a custom, made-from-scratch game client for MapleStory. It is a free, open-source project developed for educational and personal use.

## Table of Contents

- [Compatibility](#compatibility)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Required Files](#required-files)
- [Dependencies](#dependencies)
- [Troubleshooting](#troubleshooting)
- [Binaries](#binaries)
- [Donations](#donations)

---

## Compatibility

| Platform | Link |
|----------|------|
| **Server** | Compatible with version 83 servers. Tested with [HeavenMS] using v229.2. |
| **Switch** | [HeavenClientNX][Switch] |
| **Linux** | [Linux branch][Linux] |
| **Web** | [MapleStory WASM][WASM] |

---

## Getting Started

### Prerequisites

- **Visual Studio 2026** Community Edition (tested with v18.2.1)
- **Windows SDK Version:** 8.1 ([Download][Windows 8.1 SDK])
- **Platform Toolset:** v140

### Build Steps

1. Open **MapleStory.sln** in Visual Studio.
2. Verify the SDK and toolset versions are set correctly (see above).
3. Build the solution: **Build** > **Build Solution** (`Ctrl + Shift + B`).
4. Run the client: **Debug** > **Start Debugging** (`F5`).

### Converting WZ Files to NX

All WZ files from the v229.2 official client must be converted to NX format and placed in the parent folder of the executable. See the [NoLifeWzToNx] README for conversion instructions.

---

## Configuration

### Build Options

Edit **MapleStory.h** to toggle build-time features:

| Option | Description |
|--------|-------------|
| `USE_ASIO` | Use Asio for networking (requires additional dependency) |
| `USE_CRYPTO` | Enable cryptography for server communication |
| `USE_NX` | Use NX files instead of WZ files |
| `USE_DEBUG` | Suppress generation of the Settings file |

### Runtime Settings

Default settings are defined in **Configuration.h**. A **Settings** file is generated after a game session with the same options. Editing either file works the same way, but **Settings** will not persist if deleted.

### Log and console windows

The client draws two windows over the game (Dear ImGui, vendored in `includes/imgui`), one per stream:

- **Log** shows what the `LOG` macro produces. Those lines are written to the **error stream** as well and appended to a rotating file, so `2> log.txt` captures the log of a session and nothing else.
- **Console** shows the commands that were entered and what they answered. That is the **input and output stream**, so `1> console.txt` captures the commands of a session and nothing else. The commands the window takes in go through the same reader as the ones typed into the terminal the client was started in (`Util/DebugConsole.h`), so prompts asking for a line work in either of them. The field the next command is typed in hints at what could still be typed: the commands the name is the beginning of, with the arguments they take and what they do, the one that would run not dimmed; while a command waits for a line, the hint says so and that `cancel` drops the prompt.

Either window dragged out of the game window becomes a window of its own, which is what lets them stay in sight next to a full screen client. A release build compiles the `LOG` macro out, so the log window stays empty there; the console window works in both.

| Setting | Default | Description |
|---------|---------|-------------|
| `LogLines` | `5000` | How many lines the log keeps in memory at most |
| `LogSeconds` | `900` | How long the log keeps a line in memory, in seconds |
| `LogFile` | `true` | Whether the log is written to a rotating file as well |
| `LogFileMB` | `8` | Size at which the log file rolls over, in megabytes |
| `DebugUIScale` | `100` | How much the two windows are scaled, in percent, on top of the scale the desktop reports |

The windows follow the scale the desktop reports for the monitor the game window is on (GLFW's content scale, so Windows per monitor DPI and the X11 scale on Linux behave the same), and ImGui re-rasterizes the font when a window is dragged to a monitor with another scale. `DebugUIScale` is for the displays which report no scale at all: `MAPLESTORY_DEBUGUISCALE=150` makes the windows half again as large there.

The file sink writes `log/client.log` and rolls it over to `client.1.log` and `client.2.log`, so the three files together hold three times `LogFileMB`. `log [on|off|clear]` shows, hides and clears the log window, `console [on|off|clear]` does the same for the console window, and `shot [file]` writes the frame the client draws next (a bitmap, `frame.bmp` by default), which is what the client is showing without asking the screen for it.

---

## Required Files

- All WZ files from the official client must be converted to NX format.
- See **NxFiles.h** for the full list of required NX files.
- See **Configuration.h** for the latest tested WZ file version (currently v229.2).

---

## Dependencies

| Category | Library |
|----------|---------|
| NX | [NoLifeNx] |
| WZ | TBA |
| Graphics | [GLFW3], [GLEW], [FreeType], [Dear ImGui] |
| Audio | [Bass] |
| Networking | [Asio] *(optional)* |

---

## Troubleshooting

If you experience in-game glitches, UI rendering issues, or other unexpected behavior, try the following:

1. **Clean Solution** in Visual Studio.
2. **Close** Visual Studio.
3. **Delete** the following files and folders:
   - `.vs/`
   - `x64/`
   - `debug.log`
   - `MapleStory.aps`
   - `Settings`
4. **Reopen** the solution.
5. **Rebuild** the solution.

---

## Binaries

The latest build ([e3e97c2][commit]) is available here: [HeavenClient v228.3.zip][archive]

---

## Donations

If you'd like to support the continued development of HeavenClient, you can [donate here][donate].

All donations go directly toward the development of this project. Please also remember to support Nexon — this project is not meant to replace anything they offer.

> **Note:** HeavenClient is and will always be free and open-source. Do **not** pay anyone for services related to this client.

<!-- Link References -->
[HeavenMS]:          https://github.com/ryantpayton/MapleStory
[Switch]:            https://github.com/lain3d/HeavenClientNX
[Linux]:             https://github.com/ryantpayton/HeavenClient/tree/linux
[WASM]:              https://github.com/nmnsnv/maplestory-wasm
[Windows 8.1 SDK]:   https://developer.microsoft.com/en-us/windows/downloads/sdk-archive
[NoLifeWzToNx]:      https://github.com/ryantpayton/NoLifeWzToNx
[NoLifeNx]:          https://github.com/ryantpayton/NoLifeNx
[GLFW3]:             http://www.glfw.org/download.html
[GLEW]:              http://glew.sourceforge.net/
[FreeType]:          http://www.freetype.org/
[Bass]:              http://www.un4seen.com/
[Dear ImGui]:        https://github.com/ocornut/imgui
[Asio]:              http://think-async.com/
[commit]:            https://github.com/ryantpayton/MapleStory-Client/commit/e3e97c23fc6a92b87356fc2484c7f8b12d71bf19
[archive]:           https://1drv.ms/u/s!Al6eadQnem68on8i7qG62UBsFXpV?e=sumYue
[donate]:            https://www.paypal.com/donate?business=MZDZLUH2UC5FE&no_recurring=0&currency_code=USD
