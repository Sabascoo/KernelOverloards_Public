cmake_minimum_required(VERSION 3.16)
project(RoverMission C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ---------------------------------------------------------------
# Paths
# ---------------------------------------------------------------
set(IMGUI_DIR  ${CMAKE_SOURCE_DIR}/imgui)
set(GLFW_INC   ${CMAKE_SOURCE_DIR}/glfw)
set(GLFW_LIB   ${CMAKE_SOURCE_DIR}/glfw-3.4.bin.WIN64/lib-mingw-w64)

# ---------------------------------------------------------------
# main_logic  (src/rover_sim.c)
# ---------------------------------------------------------------
add_executable(main_logic src/rover_sim.c)
target_link_libraries(main_logic PRIVATE m)

# ---------------------------------------------------------------
# gui  (src/gui.cpp + ImGui + GLFW/OpenGL3)
# ---------------------------------------------------------------
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/imgui_impl_opengl3.cpp
)

add_executable(gui src/gui.cpp ${IMGUI_SOURCES})

target_include_directories(gui PRIVATE
    ${IMGUI_DIR}
    ${GLFW_INC}
)

target_link_libraries(gui PRIVATE
    ${GLFW_LIB}/libglfw3.a
    opengl32
    gdi32
    user32
    shell32
)

# ---------------------------------------------------------------
# launcher  (src/launcher.c)
# ---------------------------------------------------------------
add_executable(launcher src/launcher.c)
target_link_libraries(launcher PRIVATE shell32)

# ---------------------------------------------------------------
# All binaries -> build-mingw/
# ---------------------------------------------------------------
set(OUT_DIR ${CMAKE_SOURCE_DIR}/build-mingw)

set_target_properties(main_logic gui launcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         ${OUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${OUT_DIR}
)
