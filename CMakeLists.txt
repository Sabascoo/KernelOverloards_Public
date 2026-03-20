cmake_minimum_required(VERSION 3.16)
project(RoverMission C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ---------------------------------------------------------------
# ImGui sources (same folder for both platforms)
# ---------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/imgui_impl_opengl3.cpp
)

# ---------------------------------------------------------------
# main_logic  (src/rover_sim.c)
# ---------------------------------------------------------------
add_executable(main_logic src/rover_sim.c)
target_link_libraries(main_logic PRIVATE m)

# ---------------------------------------------------------------
# gui  (src/gui.cpp + ImGui + GLFW/OpenGL3)
# ---------------------------------------------------------------
add_executable(gui src/gui.cpp ${IMGUI_SOURCES})
target_include_directories(gui PRIVATE ${IMGUI_DIR})

if(WIN32)
    # Windows: use bundled GLFW binary
    set(GLFW_INC ${CMAKE_SOURCE_DIR}/glfw)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/glfw-3.4.bin.WIN64/lib-mingw-w64/libglfw3.a)

    target_include_directories(gui PRIVATE ${GLFW_INC})
    target_link_libraries(gui PRIVATE ${GLFW_LIB} opengl32 gdi32 user32 shell32)
else()
    # Linux/Mac: use system GLFW
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)

    target_link_libraries(gui PRIVATE glfw OpenGL::GL dl)
endif()

# ---------------------------------------------------------------
# launcher  (src/launcher.c)
# ---------------------------------------------------------------
add_executable(launcher src/launcher.c)

if(WIN32)
    target_link_libraries(launcher PRIVATE shell32)
endif()

# ---------------------------------------------------------------
# Output directory
# ---------------------------------------------------------------
if(WIN32)
    set(OUT_DIR ${CMAKE_SOURCE_DIR}/build-mingw)
else()
    set(OUT_DIR ${CMAKE_SOURCE_DIR}/build)
endif()

# main_logic and gui go into build dir
set_target_properties(main_logic gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         ${OUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${OUT_DIR}
)

# launcher stays in the project root so it never locks itself during rebuild
set_target_properties(launcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}
)
