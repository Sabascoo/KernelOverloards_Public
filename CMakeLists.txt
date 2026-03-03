cmake_minimum_required(VERSION 3.16)
project(MissionControl C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# --- main logic ---
add_executable(main_logic main.c)

# --- GUI ---
add_executable(gui
    gui.cpp
    imgui.cpp
    imgui_draw.cpp
    imgui_widgets.cpp
    imgui_tables.cpp
    imgui_impl_glfw.cpp
    imgui_impl_opengl3.cpp
)

# ImGui backend include-hoz tipikusan kell:
target_include_directories(gui PRIVATE .)

# GLFW + OpenGL
find_package(OpenGL REQUIRED)

# GLFW: Linuxon/macOS-en gyakran glfw3 config csomag van, Windows-on vcpkg / msys2 / stb.
find_package(glfw3 CONFIG QUIET)
if(glfw3_FOUND)
    target_link_libraries(gui PRIVATE glfw OpenGL::GL)
else()
    # fallback: pkg-config (Linux)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_search_module(GLFW REQUIRED glfw3)
        target_include_directories(gui PRIVATE ${GLFW_INCLUDE_DIRS})
        target_link_libraries(gui PRIVATE ${GLFW_LIBRARIES} OpenGL::GL)
        target_link_directories(gui PRIVATE ${GLFW_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "GLFW not found. Install glfw3 dev package or use vcpkg/msys2.")
    endif()
endif()

# Windows extra libs (ha MinGW/MSYS2 jelleg)
if(WIN32)
    target_link_libraries(gui PRIVATE gdi32 imm32 comdlg32 shell32)
endif()

# Linux extra libs (sokszor kell)
if(UNIX AND NOT APPLE)
    target_link_libraries(gui PRIVATE dl pthread)
endif()