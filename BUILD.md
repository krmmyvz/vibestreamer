# Vibestreamer — Derleme Kılavuzu

## Bağımlılıklar

### Linux (Arch / Manjaro)
```bash
sudo pacman -S mpv qt6-base qt6-networkauth cmake
```

### Linux (Ubuntu / Debian)
```bash
sudo apt install libmpv-dev qt6-base-dev libqt6opengl6-dev \
     libqt6network6 libqt6xml6 cmake build-essential
```

### macOS (Homebrew)
```bash
brew install mpv qt cmake
```

### Windows
1. Qt6 SDK: https://www.qt.io/download
2. libmpv: https://sourceforge.net/projects/mpv-player-windows/files/libmpv/
   - Zip'i aç, `include/` ve `lib/` klasörlerinin yolunu not et
3. CMake: https://cmake.org/download/

---

## Derleme

### Linux / macOS
```bash
cd vibestreamer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Vibestreamer
```

### Windows (MSVC)
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMPV_DIR=C:\path\to\mpv-dev
cmake --build build --config Release
.\build\Release\Vibestreamer.exe
```

---

## Proje Yapısı

```
vibestreamer/
├── CMakeLists.txt
└── src/
    ├── main.cpp            — Giriş noktası
    ├── models.h            — Veri modelleri (Source, Category, Channel, EpgProgram)
    ├── config.h/cpp        — JSON config yönetimi (~/.config/Vibestreamer/)
    ├── xtreamclient.h/cpp  — Async Xtream Codes API (QNetworkAccessManager)
    ├── m3uparser.h/cpp     — M3U/M3U8 playlist parser
    ├── epgmanager.h/cpp    — XMLTV EPG parser (QXmlStreamReader)
    ├── mpvwidget.h/cpp     — libmpv QOpenGLWidget entegrasyonu
    ├── mainwindow.h/cpp    — Ana pencere (sidebar + oynatıcı + EPG)
    ├── sourcedialog.h/cpp  — Kaynak ekle/düzenle dialog
    └── settingsdialog.h/cpp — Tercihler dialog
```

## Kısayollar

| Tuş | İşlev |
|-----|-------|
| F11 / Çift tık | Tam ekran |
| Space | Oynat / Duraklat |
| ← / → | 10 sn geri / ileri |
| ↑ / ↓ | Ses +5 / -5 |
| M | Sessiz |
| F | Favorilere ekle/çıkar |
| Esc | Tam ekrandan çık |
