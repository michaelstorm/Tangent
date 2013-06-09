######################
# PROJECT SETUP
######################
cmake_minimum_required(VERSION 2.8)

project(Chord)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_INCLUDE_CURRENT_DIR on)
set(CMAKE_BUILD_TYPE Debug)

######################
# PROTOBUF
######################
file(GLOB PROTOBUF_C_CHORD_MESSAGES "src/*.proto")
file(GLOB PROTOBUF_C_FILE_CLIENT_MESSAGES "file_client/*.proto")

find_package(ProtobufC REQUIRED)
include_directories(${PROTOBUF_C_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories("include/")
include_directories("src/")
PROTOBUF_C_GENERATE_C(PROTO_C_CHORD_SRCS PROTO_C_CHORD_HDRS ${PROTOBUF_C_CHORD_MESSAGES})
PROTOBUF_C_GENERATE_C(PROTO_C_CLIENT_SRCS PROTO_C_CLIENT_HDRS ${PROTOBUF_C_FILE_CLIENT_MESSAGES})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/include/chord/messages.pb-c.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/messages.pb-c.h ${CMAKE_CURRENT_SOURCE_DIR}/include/chord/messages.pb-c.h
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/src/messages.pb-c.h)

######################
# PROJECT SOURCES
######################
file(GLOB lib_sources "src/*.c" "src/*.h" "include/*.h" "src/logger/*.c" "src/logger/*.h")
list(APPEND lib_sources ${PROTO_C_CHORD_SRCS} ${PROTO_C_CHORD_HDRS})

######################
# LIBRARIES
######################
find_package(OpenSSL)
include_directories(${OPENSSL_INCLUDE_DIR})
set(CMAKE_C_FLAGS "-std=c99 -Wall -Wno-format-security -ggdb -fdump-rtl-expand -fvisibility=hidden -D_GNU_SOURCE")

find_package(LibEvent)
include_directories(${LIBEVENT_INCLUDE_DIR})

add_library(chord SHARED ${lib_sources})
target_link_libraries(chord ${OPENSSL_LIBRARIES} ${LIBEVENT_LIBRARIES} ${PROTOBUF_C_LIBRARY} dl m confuse)

######################
# SIMPLE CLIENT
######################
file(GLOB simple_client_sources "simple_client/*.c" "simple_client/*.h")
add_executable(simple_client ${simple_client_sources})
target_link_libraries(simple_client chord)
target_link_libraries(simple_client pthread)

######################
# FILE CLIENT
######################
file(GLOB file_client_sources "file_client/*.cpp" "file_client/*.h")
list(APPEND file_client_sources ${PROTO_C_CLIENT_SRCS} ${PROTO_C_CLIENT_HDRS})
add_executable(file_client ${file_client_sources})
target_link_libraries(file_client chord)
target_link_libraries(file_client udt)
target_link_libraries(file_client pthread)
