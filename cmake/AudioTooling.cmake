# AudioTooling.cmake â€” builds the SoLoud static library the engine needs.
# SoLoud's repo has NO root CMakeLists.txt (only the heavy contrib/ one that
# compiles every audiosource + requires libopenmpt and doesn't cover miniaudio),
# so we build exactly the subset we need: core mixer + WAV/OGG decode + one
# output backend per platform.
#
# Usage: include() this file AFTER SoLoud is fetched (soloud_SOURCE_DIR must be
# set, e.g. via FetchContent_MakeAvailable(soloud)). Creates the `soloud` target.
#
# Backends (selected by platform):
#   - Windows desktop        -> WASAPI  (WITH_WASAPI, links ole32 + avrt)
#   - Linux/macOS (CI, no device) -> Null (WITH_NULL)
#   - Emscripten (web)       -> MiniAudio -> WebAudio (WITH_MINIAUDIO,
#                              compiled with -std=gnu++20; miniaudio forbids
#                              strict -std=c* / -ansi under Emscripten)

if(TARGET soloud)
    return()
endif()

if(NOT DEFINED soloud_SOURCE_DIR)
    message(FATAL_ERROR "AudioTooling.cmake: soloud_SOURCE_DIR is not set (fetch SoLoud first)")
endif()

set(_sl_core
    ${soloud_SOURCE_DIR}/src/core/soloud.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_audiosource.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_bus.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_3d.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_basicops.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_faderops.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_filterops.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_getters.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_setters.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_voicegroup.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_core_voiceops.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_fader.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_fft.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_fft_lut.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_file.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_filter.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_misc.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_queue.cpp
    ${soloud_SOURCE_DIR}/src/core/soloud_thread.cpp
)

# WAV/MP3/FLAC/OGG decode (dr_impl.cpp + stb_vorbis.c are vendored in soloud).
set(_sl_wav
    ${soloud_SOURCE_DIR}/src/audiosource/wav/soloud_wav.cpp
    ${soloud_SOURCE_DIR}/src/audiosource/wav/soloud_wavstream.cpp
    ${soloud_SOURCE_DIR}/src/audiosource/wav/dr_impl.cpp
    ${soloud_SOURCE_DIR}/src/audiosource/wav/stb_vorbis.c
)

set(_sl_backend)
set(_sl_defs)
set(_sl_libs)

if(EMSCRIPTEN)
    list(APPEND _sl_backend ${soloud_SOURCE_DIR}/src/backend/miniaudio/soloud_miniaudio.cpp)
    list(APPEND _sl_defs WITH_MINIAUDIO)
    # miniaudio.h forbids strict -std=c* / -ansi on Emscripten builds; the
    # project may force -std=c++NN, so re-enable GNU extensions for this TU.
    # (Applied per-file: a target-wide flag would also hit stb_vorbis.c, which
    # is compiled as C and rejects -std=gnu++20.)
    set_source_files_properties(${soloud_SOURCE_DIR}/src/backend/miniaudio/soloud_miniaudio.cpp
        PROPERTIES COMPILE_OPTIONS "-std=gnu++20")
elseif(WIN32)
    list(APPEND _sl_backend ${soloud_SOURCE_DIR}/src/backend/wasapi/soloud_wasapi.cpp)
    list(APPEND _sl_defs WITH_WASAPI)
    list(APPEND _sl_libs ole32 avrt)
else()
    list(APPEND _sl_backend ${soloud_SOURCE_DIR}/src/backend/null/soloud_null.cpp)
    list(APPEND _sl_defs WITH_NULL)
endif()

add_library(soloud STATIC ${_sl_core} ${_sl_wav} ${_sl_backend})

target_include_directories(soloud PUBLIC ${soloud_SOURCE_DIR}/include)

if(_sl_defs)
    target_compile_definitions(soloud PRIVATE ${_sl_defs})
endif()

if(_sl_libs)
    target_link_libraries(soloud PRIVATE ${_sl_libs})
endif()

set_target_properties(soloud PROPERTIES CXX_VISIBILITY_PRESET hidden)

# stb_vorbis.c is C code and relies on C string-literal semantics (the SoLoud
# filehack maps fopen/fopen_s to char* mode args); compile it as C.
enable_language(C)
set_source_files_properties(${soloud_SOURCE_DIR}/src/audiosource/wav/stb_vorbis.c
    PROPERTIES LANGUAGE C)
