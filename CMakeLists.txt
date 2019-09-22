cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(dlgrind CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(BUILD_TESTING "" OFF)
add_subdirectory(capnproto)

set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CAPNPC_OUTPUT_DIR})
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
  src/dlgrind/schema.capnp
  src/dlgrind/hopcroft.capnp
  )

find_package(OpenMP)

add_library(dlgrind
  src/dlgrind/hopcroft.cpp
  src/dlgrind/hopcroft.h
  src/dlgrind/simulator.cpp
  src/dlgrind/simulator.h
  src/dlgrind/state.cpp
  src/dlgrind/state.h
  src/dlgrind/main.h
  src/dlgrind/action_string.h
  src/dlgrind/action_string.cpp
  src/magic_enum.h
  ${CAPNP_SRCS}
  ${CAPNP_HDRS}
  )
target_link_libraries(dlgrind PUBLIC capnp OpenMP::OpenMP_CXX)
target_include_directories(
  dlgrind
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/src
)

add_executable(dlgrind-opt src/dlgrind-opt.cpp)
target_link_libraries(dlgrind-opt dlgrind)

add_executable(dlgrind-rotation src/dlgrind-rotation.cpp)
target_link_libraries(dlgrind-rotation dlgrind)
