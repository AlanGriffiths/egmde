cmake_minimum_required(VERSION 3.0)
project(egmde)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC -Wnon-virtual-dtor -std=c++14")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs")

include(FindPkgConfig)

pkg_check_modules(MIRAL miral REQUIRED)

include_directories(include SYSTEM ${MIRAL_INCLUDE_DIRS})

message(STATUS "MIRAL_INCLUDE_DIRS=${MIRAL_INCLUDE_DIRS}")

add_executable(egmde egmde.cpp egwallpaper.cpp egwallpaper.h)

target_link_libraries(egmde ${MIRAL_LDFLAGS})
