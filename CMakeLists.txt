cmake_minimum_required(VERSION 3.16)
project(audio_visualizer)

# Check if build type is provided, else set to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Configure compiler flags for different build types
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add SDL2 for cross-platform window creation
find_package(SDL2 REQUIRED)

# Add OpenGL
find_package(OpenGL REQUIRED)

# Download and build dependencies
include(FetchContent)

# gl3w
FetchContent_Declare(
    gl3w
    GIT_REPOSITORY https://github.com/skaslev/gl3w
    GIT_TAG master
)
FetchContent_MakeAvailable(gl3w)

# Configure gl3w
execute_process(
    COMMAND python3 gl3w_gen.py
    WORKING_DIRECTORY ${gl3w_SOURCE_DIR}
)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot
    GIT_TAG v0.16
)
FetchContent_MakeAvailable(implot)

# Add source files
add_executable(audio_visualizer
    src/main.cpp
    src/audio_processor.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${gl3w_SOURCE_DIR}/src/gl3w.c
)

target_include_directories(audio_visualizer PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${gl3w_SOURCE_DIR}/include
    src
)

target_link_libraries(audio_visualizer PRIVATE
    ${SDL2_LIBRARIES}
    OpenGL::GL
    dl
)