cmake_minimum_required(VERSION 2.8)

project(Chord)

set(CMAKE_INCLUDE_CURRENT_DIR on)

file(GLOB lib_sources "*.c")

find_package(OpenSSL)
include_directories(${OPENSSL_INCLUDE_DIR})
set(CMAKE_C_FLAGS "-ggdb -fdump-rtl-expand")

add_library(chord SHARED ${lib_sources})
target_link_libraries(chord ${OPENSSL_LIBRARIES} dl)

file(GLOB test_sources "test/*.c")
add_executable(test ${test_sources})

file(GLOB fingers_sources "chord_getfingers/*.c")
add_executable(fingers ${fingers_sources})

target_link_libraries(test chord)
target_link_libraries(fingers chord)
