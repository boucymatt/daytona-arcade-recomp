cmake_minimum_required(VERSION 3.20)
# Run the ROM importers/recompilers on the host, never on the PSP.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(NOT DEFINED ENV{PSPDEV})
        message(FATAL_ERROR "Set PSPDEV to the public PSPDEV toolchain installation")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "$ENV{PSPDEV}/psp/share/pspdev.cmake" CACHE FILEPATH "PSPDEV toolchain")
endif()
project(daytona_psp LANGUAGES C CXX)
if(NOT PSP)
    message(FATAL_ERROR "This directory requires the PSPDEV toolchain, not a desktop compiler")
endif()
get_filename_component(DAYTONA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# PSP-1000 has a 24 MiB user partition shared by executable, heap and stacks.
# Favor code size; retain exceptions and RTTI used by the checked runtime.
add_compile_options(-Os -Wall -Wextra -fno-fast-math -ffp-contract=off
    -ffunction-sections -fdata-sections)

option(DAYTONA_PSP_COMPILE_CHECK "Compile runtime/frontend without ROM-derived code; produces no EBOOT.PBP" OFF)
set(DAYTONA_GEN_ROOT "${DAYTONA_ROOT}/build/gen" CACHE PATH "Host-generated game C++ root")
if(NOT DAYTONA_PSP_COMPILE_CHECK)
    foreach(path daytona93/gen_table.cpp daytona93_tgp/tgp_gen.cpp daytona93_snd/snd_gen.cpp)
        if(NOT EXISTS "${DAYTONA_GEN_ROOT}/${path}")
            message(FATAL_ERROR "Missing ${DAYTONA_GEN_ROOT}/${path}. Run scripts/recompile.py with the HOST compiler and your own ROMs first.")
        endif()
    endforeach()
endif()

set(SOFTFLOAT_DIR "${DAYTONA_ROOT}/extern/softfloat")
set(YMFM_DIR "${DAYTONA_ROOT}/extern/ymfm/src")
foreach(path "${SOFTFLOAT_DIR}/source/softfloat_state.c" "${YMFM_DIR}/ymfm_opn.cpp")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing dependency ${path}. Run scripts/setup.py --no-build on the host first.")
    endif()
endforeach()
include("${DAYTONA_ROOT}/cmake/softfloat_sources.cmake")
add_library(psp_softfloat STATIC ${SOFTFLOAT_SOURCES})
target_include_directories(psp_softfloat PUBLIC "${SOFTFLOAT_DIR}/source/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/softfloat" "${SOFTFLOAT_DIR}/source/8086-SSE")
target_compile_definitions(psp_softfloat PUBLIC SOFTFLOAT_FAST_INT64 LITTLEENDIAN=1 THREAD_LOCAL=
    PRIVATE SOFTFLOAT_ROUND_ODD INLINE_LEVEL=5 SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32)
# Only the board/main thread uses SoftFloat; native audio never calls it.
target_compile_options(psp_softfloat PRIVATE -w)

add_library(psp_i960 STATIC
    "${DAYTONA_ROOT}/src/i960/isa.cpp" "${DAYTONA_ROOT}/src/i960/decode.cpp"
    "${DAYTONA_ROOT}/src/i960/reach.cpp" "${DAYTONA_ROOT}/src/i960/fp.cpp")
target_include_directories(psp_i960 PUBLIC "${DAYTONA_ROOT}/src")
target_link_libraries(psp_i960 PUBLIC psp_softfloat)
add_library(psp_trace STATIC "${DAYTONA_ROOT}/src/trace/trace.cpp")
target_include_directories(psp_trace PUBLIC "${DAYTONA_ROOT}/src")
add_library(psp_ymfm STATIC "${YMFM_DIR}/ymfm_opn.cpp" "${YMFM_DIR}/ymfm_adpcm.cpp" "${YMFM_DIR}/ymfm_ssg.cpp")
target_include_directories(psp_ymfm SYSTEM PUBLIC "${YMFM_DIR}")
target_compile_options(psp_ymfm PRIVATE -w)

set(RUNTIME_FILES enhance cpu tgp m2_tgp_board geo raster video m2_board game_loop zip rom_import
    archive m2_replay_bus lockstep snd_cpu multipcm sound_board paged_rom
    native_sample_mixer native_sound_sequencer native_sound_engine)
set(RUNTIME_SOURCES)
foreach(name IN LISTS RUNTIME_FILES)
    list(APPEND RUNTIME_SOURCES "${DAYTONA_ROOT}/src/runtime/${name}.cpp")
endforeach()
# Keep generated game code size-focused, but optimize the measured pixel,
# geometry, ROM-cache and sample loops for speed. IEEE/rounding flags above
# remain in force; no fast-math or guest scheduling changes.
foreach(hot_source video raster geo paged_rom native_sample_mixer native_sound_sequencer)
    set_source_files_properties("${DAYTONA_ROOT}/src/runtime/${hot_source}.cpp"
        PROPERTIES COMPILE_OPTIONS "-O3")
endforeach()
add_library(psp_runtime STATIC ${RUNTIME_SOURCES})
target_include_directories(psp_runtime PUBLIC "${DAYTONA_ROOT}/src")
target_compile_definitions(psp_runtime PUBLIC M2_LOW_MEMORY=1 M2_PSP_NATIVE_VIDEO=1)
target_link_libraries(psp_runtime PUBLIC psp_trace psp_i960 psp_ymfm)
# No M2_VITA_RENDER_OPT: its large tile caches are unsuitable for PSP-1000.
# No SDL or reference interpreter. GU/input/audio use public native PSPSDK APIs.
set(FRONTEND_SOURCES main.cpp rom_loader.cpp)
if(DAYTONA_PSP_COMPILE_CHECK)
    add_library(daytona_psp_frontend OBJECT ${FRONTEND_SOURCES})
    target_link_libraries(daytona_psp_frontend PRIVATE psp_runtime)
    message(STATUS "ROM-free COMPILE CHECK only: no linked game or EBOOT.PBP will be produced")
else()
    file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${DAYTONA_GEN_ROOT}/daytona93/*.cpp")
    list(APPEND GEN_SOURCES "${DAYTONA_GEN_ROOT}/daytona93_tgp/tgp_gen.cpp"
        "${DAYTONA_GEN_ROOT}/daytona93_snd/snd_gen.cpp")
    add_executable(daytona_psp ${FRONTEND_SOURCES} ${GEN_SOURCES})
    # Resolve C++/runtime archives before SDK imports, keeping each import's
    # stubs contiguous. GCC supplies pspsdk/pspuser last in its default specs.
    target_link_libraries(daytona_psp PRIVATE psp_runtime psp_trace psp_i960
        psp_softfloat psp_ymfm stdc++ atomic m pspdebug pspgum pspgu pspaudio
        pspctrl psppower pspdisplay pspge)
    target_link_options(daytona_psp PRIVATE -Wl,--gc-sections
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/daytona_psp.map")
    set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")

    # CreatePBP.cmake treats MEMSIZE=0 as an omitted value and substitutes 1.
    # Supply our own SFO so even later models use the PSP-1000 memory budget.
    set(PSP_SFO "${CMAKE_CURRENT_BINARY_DIR}/PARAM.SFO")
    add_custom_command(OUTPUT "${PSP_SFO}"
        COMMAND "${PSPDEV}/bin/mksfoex" -d MEMSIZE=0 -s APP_VER=01.00
            "Daytona Recomp PSP" "${PSP_SFO}"
        VERBATIM)
    add_custom_target(daytona_psp_sfo DEPENDS "${PSP_SFO}")
    add_dependencies(daytona_psp daytona_psp_sfo)
    create_pbp_file(TARGET daytona_psp TITLE "Daytona Recomp PSP"
        VERSION 01.00 BUILD_PRX SFO_PATH "${PSP_SFO}")
endif()
