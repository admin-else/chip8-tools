cmake_minimum_required(VERSION 3.26.0)
project(Chip8E LANGUAGES C)

find_package(raylib REQUIRED)

add_executable(Chip8E chip8e.c)
target_link_libraries(Chip8E raylib)
