cmake_minimum_required(VERSION 3.22...3.25)


project(obs_sample)

set(CMAKE_CXX_STANDARD 17)

set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/output)
set(LIBRARY_OUTPUT_PATH ${OUTPUT_DIR})
set(EXECUTABLE_OUTPUT_PATH ${OUTPUT_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")


find_package(jansson REQUIRED)
find_package(FFmpeg REQUIRED avformat avutil swscale swresample OPTIONAL_COMPONENTS avcodec)

find_package(ZLIB REQUIRED)
find_package(LibUUID REQUIRED)
find_package(X11 REQUIRED)
find_package(x11_xcb REQUIRED)
# cmake-format: off
find_package(xcb COMPONENTS xcb OPTIONAL_COMPONENTS xcb-xinput QUIET)
# cmake-format: on
#find_package(gio)
find_package(Threads REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libobs)

add_subdirectory(deps)
add_subdirectory(libobs)
add_subdirectory(plugins)
add_subdirectory(example)