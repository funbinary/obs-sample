
configure_file(obsconfig.h.in "${CMAKE_CURRENT_SOURCE_DIR}/obsconfig.h" @ONLY)
add_subdirectory(libcaption)
add_subdirectory(uthash)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} OBS_FILES)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/callback OBS_FILES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/util OBS_FILES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/graphics OBS_FILES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/graphics/libnsgif OBS_FILES)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/media OBS_FILES)


add_library(libobs SHARED)
add_library(OBS::libobs ALIAS libobs)
target_sources(
        libobs
        PRIVATE
        ${OBS_FILES}
)

target_link_libraries(
        libobs
        PRIVATE OBS::caption
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
        PUBLIC Threads::Threads)