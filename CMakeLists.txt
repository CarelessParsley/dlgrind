cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(dlgrind CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_TESTING "" OFF)
add_subdirectory(capnproto)

set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CAPNPC_OUTPUT_DIR})
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS dlgrind/schema.capnp)

add_library(dlgrind
  dlgrind/hopcroft.cpp
  dlgrind/hopcroft.h
  dlgrind/simulator.cpp
  dlgrind/simulator.h
  dlgrind/state.cpp
  dlgrind/state.h
  magic_enum.h
  ${CAPNP_SRCS}
  ${CAPNP_HDRS}
  )
target_link_libraries(dlgrind PUBLIC capnp)
target_include_directories(
  dlgrind
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(dlgrind-opt dlgrind-opt.cpp)
target_link_libraries(dlgrind-opt dlgrind)

add_executable(dlgrind-rotation dlgrind-rotation.cpp)
target_link_libraries(dlgrind-rotation dlgrind)
