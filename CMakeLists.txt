cmake_minimum_required(VERSION 3.20)
project(Vibestreamer VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Xml OpenGL OpenGLWidgets Concurrent Svg)
find_package(ZLIB REQUIRED)

# libmpv detection — pkg-config first, then manual find, then MPV_DIR override
set(MPV_DIR "" CACHE PATH "Manual path to libmpv (contains include/ and lib/). Set on Windows or when pkg-config fails.")

if(NOT MPV_DIR STREQUAL "")
    # Manual override (Windows or custom installs)
    set(MPV_INCLUDE_DIRS "${MPV_DIR}/include")
    find_library(MPV_LIB 
        NAMES mpv libmpv libmpv.dll.a
        PATHS "${MPV_DIR}/lib" "${MPV_DIR}" 
        REQUIRED
    )
    set(MPV_LIBRARIES "${MPV_LIB}")
    message(STATUS "libmpv: using MPV_DIR=${MPV_DIR}")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MPV QUIET mpv)
    endif()

    if(NOT MPV_FOUND)
        # pkg-config failed or not available — try finding headers + lib directly
        find_path(MPV_INCLUDE_DIR mpv/client.h
            PATHS /usr/include /usr/local/include /opt/homebrew/include)
        find_library(MPV_LIB mpv
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib
                  /usr/lib/x86_64-linux-gnu /usr/lib64)
        if(MPV_INCLUDE_DIR AND MPV_LIB)
            set(MPV_FOUND TRUE)
            set(MPV_INCLUDE_DIRS "${MPV_INCLUDE_DIR}")
            set(MPV_LIBRARIES    "${MPV_LIB}")
            message(STATUS "libmpv: found via filesystem search")
            message(STATUS "  headers : ${MPV_INCLUDE_DIR}")
            message(STATUS "  library : ${MPV_LIB}")
        else()
            message(FATAL_ERROR
                "libmpv not found.\n"
                "Linux  : sudo pacman -S mpv   (or apt install libmpv-dev)\n"
                "macOS  : brew install mpv\n"
                "Windows: set -DMPV_DIR=<path-to-mpv-dev>")
        endif()
    else()
        message(STATUS "libmpv: found via pkg-config (${MPV_VERSION})")
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/xtreamclient.cpp
    src/m3uparser.cpp
    src/epgmanager.cpp
    src/mpvwidget.cpp
    src/mainwindow.cpp
    src/sourcedialog.cpp
    src/settingsdialog.cpp
    src/multiviewdialog.cpp
    src/imagecache.cpp
    resources.qrc
)

qt_add_executable(Vibestreamer ${SOURCES})

target_include_directories(Vibestreamer PRIVATE src ${MPV_INCLUDE_DIRS})

if(MPV_LIBRARY_DIRS)
    target_link_directories(Vibestreamer PRIVATE ${MPV_LIBRARY_DIRS})
endif()

target_link_libraries(Vibestreamer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Xml
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Concurrent
    Qt6::Svg
    ${MPV_LIBRARIES}
    ZLIB::ZLIB
)

# macOS: link OpenGL + IOKit frameworks
if(APPLE)
    target_link_libraries(Vibestreamer PRIVATE
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

# Windows: hide console window in Release builds
if(WIN32)
    set_target_properties(Vibestreamer PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()

install(TARGETS Vibestreamer DESTINATION bin)
