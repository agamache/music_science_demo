cmake_minimum_required(VERSION 3.20)
project(music_science_demo C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2)

file(GLOB IMGUI_SOURCES imgui/*.cpp)
list(APPEND IMGUI_SOURCES imgui/backends/imgui_impl_sdl2.cpp imgui/backends/imgui_impl_sdlrenderer2.cpp)

include_directories(imgui imgui/backends)

add_executable(music_science_demo src/main.cpp ${IMGUI_SOURCES})

target_link_libraries(music_science_demo SDL2::SDL2)