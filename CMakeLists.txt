cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0022 NEW)

project(egmde)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC -Wnon-virtual-dtor -std=c++17")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs")

include(FindPkgConfig)
pkg_check_modules(MIRAL miral REQUIRED)
pkg_check_modules(FREETYPE freetype2 REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(XKBCOMMON xkbcommon REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)

pkg_check_modules(MIRWAYLAND mirwayland REQUIRED)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)
#pkg_check_modules(MIRAL mircommon REQUIRED) # Mir bug #868
set(MIRCOMMON_INCLUDE_DIRS /usr/include/mircommon/)
set(MIRCOMMON_LDFLAGS "-lmircommon")

add_subdirectory(wayland-generated)

add_executable(egmde
    egmde.cpp
    eglauncher.cpp eglauncher.h
    egwallpaper.cpp egwallpaper.h
    egwindowmanager.cpp egwindowmanager.h
    egworker.cpp egworker.h
    printer.cpp printer.h
    egfullscreenclient.cpp egfullscreenclient.h
    egshellcommands.cpp egshellcommands.h
    wayland-generated/primary-selection-unstable-v1_wrapper.cpp
    wayland-generated/primary-selection-unstable-v1_wrapper.h
    wayland-generated/gtk-primary-selection_wrapper.cpp
    wayland-generated/gtk-primary-selection_wrapper.h
    egprimary_selection_device_controller.cpp egprimary_selection_device_controller.h
    primary_selection.cpp primary_selection.h
    gtk_primary_selection.cpp gtk_primary_selection.h
)

execute_process(
    COMMAND grep ^NAME= /etc/os-release 
    COMMAND cut -d= -f2
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE EGMDE_OS)


if (EGMDE_OS STREQUAL "Ubuntu")
  set(EGMDE_WALLPAPER_BOTTOM 0x92006a)
elseif (EGMDE_OS STREQUAL "Fedora")
  set(EGMDE_WALLPAPER_BOTTOM 0x25487c)
else()
  set(EGMDE_WALLPAPER_BOTTOM 0x92006a)
endif()

set_source_files_properties(egmde.cpp PROPERTIES COMPILE_DEFINITIONS EGMDE_WALLPAPER_BOTTOM="${EGMDE_WALLPAPER_BOTTOM}")

target_include_directories(egmde PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS} /usr/include/mirwayland/ ${MIRCOMMON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(egmde
  ${MIRAL_LDFLAGS}
  ${MIRWAYLAND_LDFLAGS}
  ${MIRCOMMON_LDFLAGS}
  ${WAYLAND_CLIENT_LIBRARIES}
  ${WAYLAND_SERVER_LDFLAGS}
  ${Boost_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${XKBCOMMON_LIBRARIES}
)

add_custom_target(egmde-launch ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/egmde-launch.sh ${CMAKE_BINARY_DIR}/egmde-launch
)

add_custom_target(egmde-terminal ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/egmde-terminal ${CMAKE_BINARY_DIR}
)

install(PROGRAMS ${CMAKE_BINARY_DIR}/egmde-launch ${CMAKE_BINARY_DIR}/egmde-terminal ${CMAKE_BINARY_DIR}/egmde
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

install(FILES ${CMAKE_SOURCE_DIR}/egmde.desktop
    DESTINATION /usr/share/wayland-sessions/
)
