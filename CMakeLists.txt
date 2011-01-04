cmake_minimum_required(VERSION 2.8)

project(Chord)

set(CMAKE_INCLUDE_CURRENT_DIR on)

file(GLOB lib_sources "*.c")

add_library(chord SHARED ${lib_sources})

file(GLOB test_sources "test/*.c")
add_executable(test ${test_sources})

target_link_libraries(test chord)
