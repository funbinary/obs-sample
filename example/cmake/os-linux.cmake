

#target_sources(
#  libobs
#  PRIVATE obs-nix.c
#          obs-nix-platform.c
#          obs-nix-platform.h
#          obs-nix-x11.c
#          util/pipe-posix.c
#          util/platform-nix.c
#          util/threading-posix.c
#          util/threading-posix.h)

#target_compile_definitions(libobs PRIVATE USE_XDG $<$<C_COMPILER_ID:GNU>:ENABLE_DARRAY_TYPE_TEST>)
target_link_libraries(obs_example PRIVATE X11::x11-xcb xcb::xcb LibUUID::LibUUID ${CMAKE_DL_LIBS})
if (TARGET xcb::xcb-xinput)
    target_link_libraries(obs_example PRIVATE xcb::xcb-xinput)
endif ()
#
#if (ENABLE_PULSEAUDIO)
#    find_package(PulseAudio REQUIRED)
#
#    target_sources(
#            libobs
#            PRIVATE audio-monitoring/pulse/pulseaudio-enum-devices.c audio-monitoring/pulse/pulseaudio-output.c
#            audio-monitoring/pulse/pulseaudio-monitoring-available.c audio-monitoring/pulse/pulseaudio-wrapper.c
#            audio-monitoring/pulse/pulseaudio-wrapper.h)
#
#    target_link_libraries(obs_example PRIVATE PulseAudio::PulseAudio)
#    target_enable_feature(obs_example "PulseAudio audio monitoring (Linux)")
#else ()
#    target_sources(obs_example PRIVATE audio-monitoring/null/null-audio-monitoring.c)
#    target_disable_feature(obs_example "PulseAudio audio monitoring (Linux)")
#endif ()
#
#if (TARGET gio::gio)
#    target_sources(obs_example PRIVATE util/platform-nix-dbus.c util/platform-nix-portal.c)
#    target_link_libraries(obs_example PRIVATE gio::gio)
#endif ()
#
#if (ENABLE_WAYLAND)
#    # cmake-format: off
#    find_package(Wayland COMPONENTS Client REQUIRED)
#    # cmake-format: on
#    find_package(xkbcommon REQUIRED)
#
#    target_sources(obs_example PRIVATE obs-nix-wayland.c)
#    target_link_libraries(obs_example PRIVATE Wayland::Client xkbcommon::xkbcommon)
#    target_enable_feature(obs_example "Wayland compositor support (Linux)")
#else ()
#    target_disable_feature(obs_example "Wayland compositor support (Linux)")
#endif ()
#
#set_target_properties(obs_example PROPERTIES OUTPUT_NAME obs)
