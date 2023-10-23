set(Obs_EXAMPLES
        obs_example
)

find_package(Jansson MODULE REQUIRED)
find_package(FFmpeg REQUIRED avformat avutil swscale swresample OPTIONAL_COMPONENTS avcodec)
find_package(ZLIB REQUIRED)
find_package(LibUUID REQUIRED)

find_package(X11 REQUIRED)
find_package(X11_XCB MODULE REQUIRED)
# cmake-format: off
find_package(xcb COMPONENTS xcb OPTIONAL_COMPONENTS xcb-xinput QUIET)
# cmake-format: on
#find_package(gio)
find_package(Threads REQUIRED)
file(GLOB Obs_EXAMPLES *.cpp)
foreach (obs_cpp_file ${Obs_EXAMPLES})
    file(RELATIVE_PATH obs_file_name "${CMAKE_CURRENT_SOURCE_DIR}" ${obs_cpp_file})
    string(REPLACE ".cpp" "" obs_name ${obs_file_name})
    add_executable(${obs_name} ${obs_cpp_file}
            obsimp.cpp
            obsimp.h
            const_define.h)
    TARGET_LINK_LIBRARIES(${obs_name}
            OBS::libobs
            OBS::caption
            OBS::uthash
            FFmpeg::avcodec
            FFmpeg::avformat
            FFmpeg::avutil
            FFmpeg::swscale
            FFmpeg::swresample
            jansson
            ZLIB::ZLIB
            X11::X11_xcb
            xcb
            LibUUID::LibUUID
    )
endforeach ()


