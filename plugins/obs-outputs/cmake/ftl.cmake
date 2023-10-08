find_package(CURL REQUIRED)
find_package(jansson REQUIRED)

add_library(ftl-sdk INTERFACE)
add_library(OBS::ftl-sdk ALIAS ftl-sdk)

target_compile_definitions(ftl-sdk INTERFACE FTL_STATIC_COMPILE FTL_FOUND)

target_link_libraries(ftl-sdk INTERFACE jansson::jansson CURL::libcurl)

target_sources(
  ftl-sdk
  INTERFACE # cmake-format: sortable
            ftl-sdk/libftl/ftl-sdk.c
            ftl-sdk/libftl/ftl.h
            ftl-sdk/libftl/ftl_helpers.c
            ftl-sdk/libftl/ftl_private.h
            ftl-sdk/libftl/gettimeofday/gettimeofday.c
            ftl-sdk/libftl/handshake.c
            ftl-sdk/libftl/hmac/hmac.c
            ftl-sdk/libftl/hmac/hmac.h
            ftl-sdk/libftl/hmac/sha2.c
            ftl-sdk/libftl/hmac/sha2.h
            ftl-sdk/libftl/ingest.c
            ftl-sdk/libftl/logging.c
            ftl-sdk/libftl/media.c
            ftl-stream.c)

target_include_directories(ftl-sdk INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ftl-sdk/libftl")

if(OS_WINDOWS)
  target_sources(
    ftl-sdk INTERFACE ftl-sdk/libftl/gettimeofday/gettimeofday.c ftl-sdk/libftl/gettimeofday/gettimeofday.h
                      ftl-sdk/libftl/win32/socket.c ftl-sdk/libftl/win32/threads.c ftl-sdk/libftl/win32/threads.h)

  target_include_directories(ftl-sdk INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ftl-sdk/libftl/win32")
else()
  target_sources(ftl-sdk INTERFACE ftl-sdk/libftl/posix/socket.c ftl-sdk/libftl/posix/threads.c
                                   ftl-sdk/libftl/posix/threads.h)

  target_include_directories(ftl-sdk INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ftl-sdk/libftl/posix")
endif()

target_link_libraries(obs-outputs PRIVATE ftl-sdk)

target_enable_feature(obs-outputs "FTL protocol support")

get_target_property(target_sources ftl-sdk INTERFACE_SOURCES)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  set(silence_ftl -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-pointer-sign -Wno-int-conversion)

  if(CMAKE_C_COMPILER_ID MATCHES "(Apple)?Clang")
    list(
      APPEND
      silence_ftl
      -Wno-incompatible-function-pointer-types
      -Wno-implicit-int-conversion
      -Wno-shorten-64-to-32
      -Wno-macro-redefined
      -Wno-enum-conversion)
  elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    list(APPEND silence_ftl -Wno-extra -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-builtin-macro-redefined)
  endif()

  if((NOT CMAKE_C_COMPILER_ID STREQUAL "GNU") OR CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    list(APPEND silence_ftl -Wno-error=enum-conversion)
  endif()
  set_source_files_properties(${target_sources} PROPERTIES COMPILE_OPTIONS "${silence_ftl}")
endif()

set(target_headers ${target_sources})
list(FILTER target_sources INCLUDE REGEX ".+ftl-sdk/.+\\.(m|c[cp]?p?|swift)")
list(FILTER target_headers INCLUDE REGEX ".+ftl-sdk/.+\\.h(pp)?")

source_group("ftl-sdk\\Source Files" FILES ${target_sources})
source_group("ftl-sdk\\Header Files" FILES ${target_headers})
