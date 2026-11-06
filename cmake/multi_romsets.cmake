# Compile unchanged generated sources in distinct namespaces. Include support
# headers before renaming so both games retain the same gen::Env/sndgen::Env
# types and share the native board/runtime implementation.
if(EXISTS "${M2_GEN_ROOT}/daytona/gen_table.cpp")
    set(_m2_revision_gen_default "${M2_GEN_ROOT}")
else()
    get_filename_component(_m2_host_build "${M2_GEN_ROOT}" DIRECTORY)
    set(_m2_revision_gen_default "${_m2_host_build}-daytona/gen")
endif()
set(M2_REV_GEN_ROOT "${_m2_revision_gen_default}" CACHE PATH
    "Directory containing recompiled daytona/daytona_tgp/daytona_snd sources")

foreach(_m2_set IN ITEMS daytona93 daytona)
    if(_m2_set STREQUAL "daytona93")
        set(_m2_root "${M2_GEN_ROOT}")
    else()
        set(_m2_root "${M2_REV_GEN_ROOT}")
    endif()
    foreach(_m2_required IN ITEMS
        "${_m2_set}/gen_table.cpp" "${_m2_set}/chunk_000.cpp"
        "${_m2_set}_tgp/tgp_gen.cpp" "${_m2_set}_snd/snd_gen.cpp")
        if(NOT EXISTS "${_m2_root}/${_m2_required}")
            message(FATAL_ERROR
                "Both ROM sets are required for M2_MULTI_ROMSETS. Missing ${_m2_root}/${_m2_required}. "
                "Run host setup with daytona93 and daytona ROM archives first, or set M2_GEN_ROOT/M2_REV_GEN_ROOT.")
        endif()
    endforeach()
endforeach()

# Both arcade revisions upload the same TGP program. Enforce identical
# generated implementations rather than silently choosing one if that changes.
set(TGP_GEN "${M2_GEN_ROOT}/daytona93_tgp/tgp_gen.cpp")
set(_m2_revision_tgp "${M2_REV_GEN_ROOT}/daytona_tgp/tgp_gen.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TGP_GEN}" "${_m2_revision_tgp}")
file(SHA256 "${TGP_GEN}" _m2_deluxe_tgp_hash)
file(SHA256 "${_m2_revision_tgp}" _m2_revision_tgp_hash)
if(NOT _m2_deluxe_tgp_hash STREQUAL _m2_revision_tgp_hash)
    message(FATAL_ERROR "Both ROM sets must use identical generated TGP code. Recompile both sets with the same options.")
endif()

set(GEN_SOURCES "${CMAKE_SOURCE_DIR}/src/runtime/generated_dispatch.cpp")
set(SND_GEN "") # both sound programs are included by the wrappers below
foreach(_m2_set IN ITEMS daytona93 daytona)
    if(_m2_set STREQUAL "daytona93")
        set(_m2_root "${M2_GEN_ROOT}")
    else()
        set(_m2_root "${M2_REV_GEN_ROOT}")
    endif()
    file(GLOB _m2_i960_sources CONFIGURE_DEPENDS "${_m2_root}/${_m2_set}/*.cpp")
    list(APPEND _m2_i960_sources "${_m2_root}/${_m2_set}_snd/snd_gen.cpp")
    foreach(_m2_generated IN LISTS _m2_i960_sources)
        get_filename_component(_m2_name "${_m2_generated}" NAME)
        set(_m2_wrapper "${CMAKE_BINARY_DIR}/multi_romsets/${_m2_set}/${_m2_name}")
        file(GENERATE OUTPUT "${_m2_wrapper}" CONTENT
"// Generated namespace wrapper; game code remains in the host build directory.
#include \"runtime/gen_support.h\"
#include \"runtime/snd_gen_support.h\"
namespace gen_${_m2_set} { using namespace ::gen; }
namespace sndgen_${_m2_set} { using namespace ::sndgen; }
#define gen gen_${_m2_set}
#define sndgen sndgen_${_m2_set}
#include \"${_m2_generated}\"
")
        list(APPEND GEN_SOURCES "${_m2_wrapper}")
    endforeach()
endforeach()
message(STATUS "One application includes Daytona Deluxe '93 and Revision A")
