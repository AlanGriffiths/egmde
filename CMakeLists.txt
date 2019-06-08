cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0022 NEW)

project(egmde)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC -Wnon-virtual-dtor -std=c++17")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs")

include(FindPkgConfig)
pkg_check_modules(MIRAL miral REQUIRED)

add_executable(egmde
    egmde.cpp
)

target_include_directories(egmde PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS})
target_link_libraries(egmde ${MIRAL_LDFLAGS})

add_custom_target(egmde-desktop ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/egmde-desktop.sh ${CMAKE_BINARY_DIR}/egmde-desktop
)

install(PROGRAMS ${CMAKE_BINARY_DIR}/egmde ${CMAKE_BINARY_DIR}/egmde-desktop
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

install(FILES ${CMAKE_SOURCE_DIR}/egmde.desktop
    DESTINATION /usr/share/wayland-sessions/
)
