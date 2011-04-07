cmake_minimum_required(VERSION 2.8)

project(Chord)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_INCLUDE_CURRENT_DIR on)

file(GLOB PROTOBUF_C_MESSAGES "*.proto")

find_package(ProtobufC REQUIRED)
include_directories(${PROTOBUF_C_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_C_GENERATE_C(PROTO_C_SRCS PROTO_C_HDRS ${PROTOBUF_C_MESSAGES})

file(GLOB lib_sources "*.c" "*.h")
list(APPEND lib_sources ${PROTO_C_SRCS} ${PROTO_C_HDRS})

find_package(OpenSSL)
include_directories(${OPENSSL_INCLUDE_DIR})
set(CMAKE_C_FLAGS "-Wall -ggdb -fdump-rtl-expand")

find_package(LibEvent)
include_directories(${LIBEVENT_INCLUDE_DIR})

add_library(chord SHARED ${lib_sources})
target_link_libraries(chord ${OPENSSL_LIBRARIES} ${LIBEVENT_LIBRARIES} ${PROTOBUF_C_LIBRARY} dl m)

file(GLOB client_sources "client/*.cpp" "client/*.h")
add_executable(client ${client_sources})

#file(GLOB fingers_sources "getfingers/*.c")
#add_executable(getfingers ${fingers_sources})

file(GLOB gen_conf_same_sources "gen_conf_same/*.c")
add_executable(gen_conf_same ${gen_conf_same_sources})

target_link_libraries(client chord)
target_link_libraries(client udt)
target_link_libraries(client pthread)
#target_link_libraries(getfingers chord)
target_link_libraries(gen_conf_same chord)
