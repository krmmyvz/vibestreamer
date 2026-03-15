# Vibestreamer

Modern, feature-rich IPTV player built with **C++17 + Qt6 + libmpv**.

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-blue)
![Qt](https://img.shields.io/badge/Qt-6-green)
![License](https://img.shields.io/badge/license-MIT-orange)

---

## ✨ Features

- 📺 **Xtream Codes API** — Live TV, VOD, and Series support
- 📋 **M3U/M3U8 Playlist** — Local or remote playlist loading with background parsing
- 📅 **EPG (Electronic Program Guide)** — XMLTV-based, real-time program info overlay
- 🎬 **libmpv Playback** — Hardware-accelerated decoding via OpenGL render context
- 🖼️ **Picture-in-Picture** — Floating mini player mode
- 📡 **Multi-View** — Multiple channels side-by-side
- ⭐ **Favorites** — Persistent channel bookmarking
- 🔴 **Screen Recording** — Direct stream recording via mpv
- 🌓 **Dark / Light Theme** — Full dynamic theme switching, no restart required
- 🔍 **Search & Filter** — Channel search with history, category browsing
- 🌐 **Bilingual UI** — Turkish and English interface
- 🖥️ **System Tray** — Minimize to tray support
- ⌨️ **Keyboard Shortcuts** — Fully configurable

---

## 📸 Screenshots

> Launch the app and use Settings → Dark/Light Theme to switch between visual modes.

---

## 🚀 Getting Started

### Dependencies

#### Linux (Arch / Manjaro)
```bash
sudo pacman -S mpv qt6-base qt6-opengl cmake
```

#### Linux (Ubuntu / Debian)
```bash
sudo apt install libmpv-dev qt6-base-dev libqt6opengl6-dev \
     libqt6svg6-dev libqt6xml6 cmake build-essential
```

#### macOS (Homebrew)
```bash
brew install mpv qt cmake
```

#### Windows
1. [Qt6 SDK](https://www.qt.io/download)
2. [libmpv](https://sourceforge.net/projects/mpv-player-windows/files/libmpv/) — extract and note the path
3. [CMake](https://cmake.org/download/)

---

### Build

#### Linux / macOS
```bash
git clone https://github.com/krmmyvz/vibestreamer.git
cd vibestreamer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Vibestreamer
```

#### Windows (MSVC)
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMPV_DIR=C:\path\to\mpv-dev
cmake --build build --config Release
.\build\Release\Vibestreamer.exe
```

---

## ⌨️ Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Space` | Play / Pause |
| `F11` / Double-click | Toggle fullscreen |
| `←` / `→` | Seek ±10 seconds |
| `↑` / `↓` | Volume ±5 |
| `M` | Toggle mute |
| `F` | Toggle favorite |
| `N` / `P` | Next / Previous channel |
| `A` | Select audio track |
| `S` | Select subtitle |
| `I` | Show media info |
| `Esc` | Exit fullscreen / PiP |

---

## 🗂️ Project Structure

```
vibestreamer/
├── CMakeLists.txt
└── src/
    ├── main.cpp              — Entry point
    ├── models.h              — Data models (Source, Category, Channel, EpgProgram)
    ├── config.h/cpp          — JSON config management (~/.config/Vibestreamer/)
    ├── theme.h               — Design token system (dark/light palettes + QSS generation)
    ├── icons.h               — SVG icon factory with dynamic tinting
    ├── xtreamclient.h/cpp    — Async Xtream Codes API client
    ├── m3uparser.h/cpp       — M3U/M3U8 playlist parser (background thread)
    ├── epgmanager.h/cpp      — XMLTV EPG parser
    ├── imagecache.h/cpp      — Async channel logo cache
    ├── mpvwidget.h/cpp       — libmpv QOpenGLWidget renderer
    ├── mainwindow.h/cpp      — Main window (sidebar + player + EPG)
    ├── sourcedialog.h/cpp    — Add/edit source dialog
    ├── settingsdialog.h/cpp  — Preferences dialog
    └── multiviewdialog.h/cpp — Multi-view dialog
```

---

## ⚙️ Configuration

Config is stored at `~/.config/Vibestreamer/config.json` and includes:
- Source definitions (Xtream Codes or M3U)
- Favorites list
- Theme preference (dark / light)
- Accent color
- Volume, language, shortcuts, and more

---

## 📝 License

MIT License © 2025 krmmyvz
