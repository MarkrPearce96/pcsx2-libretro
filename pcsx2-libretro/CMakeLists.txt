# pcsx2-libretro — libretro core frontend for PCSX2.
#
# Built only when -DENABLE_LIBRETRO=ON is passed. Off by default so this
# directory is invisible to anyone building upstream PCSX2 normally.
#
# Modeled on pcsx2-gsrunner/CMakeLists.txt — same minimal shape, but
# produces a MODULE library (loadable plugin) rather than an executable.

add_library(pcsx2_libretro MODULE)

target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
    LibretroAudioStream.cpp
    LibretroInputSource.cpp
    LibretroSaveState.cpp
)

target_include_directories(pcsx2_libretro PRIVATE
    "${CMAKE_BINARY_DIR}/common/include"
    "${CMAKE_SOURCE_DIR}/pcsx2"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(pcsx2_libretro PRIVATE
    PCSX2_FLAGS
    PCSX2
)

# Libretro naming convention: pcsx2_libretro.dylib (no "lib" prefix).
set_target_properties(pcsx2_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pcsx2_libretro"
)

# Hide symbols by default — libretro cores only export the retro_* C ABI.
# The retro_* functions get explicit visibility from RETRO_API in libretro.h.
set_target_properties(pcsx2_libretro PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(PACKAGE_MODE)
    install(TARGETS pcsx2_libretro LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
    install(TARGETS pcsx2_libretro LIBRARY DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()
