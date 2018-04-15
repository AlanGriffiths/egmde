cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0022 NEW)

project(egmde)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC -Wnon-virtual-dtor -std=c++17")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs")

include(FindPkgConfig)
pkg_check_modules(MIRAL miral REQUIRED)
pkg_check_modules(FREETYPE freetype2 REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)

add_executable(egmde
    egmde.cpp
    eglauncher.cpp eglauncher.h
    egwallpaper.cpp egwallpaper.h
    egwindowmanager.cpp egwindowmanager.h
    egworker.cpp egworker.h
    printer.cpp printer.h
)

target_include_directories(egmde PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(     egmde               ${MIRAL_LDFLAGS}      ${Boost_LIBRARIES}    ${FREETYPE_LIBRARIES})

add_custom_target(egmde-desktop ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/egmde-desktop.sh ${CMAKE_BINARY_DIR}/egmde-desktop
)

add_custom_target(egmde-app ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/egmde-app.sh ${CMAKE_BINARY_DIR}/egmde-app
)

install(PROGRAMS ${CMAKE_BINARY_DIR}/egmde-desktop ${CMAKE_BINARY_DIR}/egmde-app
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)
