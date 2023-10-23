set(Obs_EXAMPLES
        obs_example
)


file(GLOB Obs_EXAMPLES *.cpp)
foreach (obs_cpp_file ${Obs_EXAMPLES})
    file(RELATIVE_PATH obs_file_name "${CMAKE_CURRENT_SOURCE_DIR}" ${obs_cpp_file})
    string(REPLACE ".cpp" "" obs_name ${obs_file_name})
    add_executable(${obs_name} ${obs_cpp_file}
            obs_ctl.cpp
            obs_ctl.h
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


