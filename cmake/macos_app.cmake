# The two ROM sets are built independently, but their app lives beside the
# shared desktop executables. A standalone Revision A build gets its own app.
set(_m2_app_output_dir "${CMAKE_BINARY_DIR}")
if(M2_ROMSET STREQUAL "daytona" AND _m2_game_dir)
    set(_m2_app_output_dir "${_m2_game_dir}")
endif()

add_executable(daytona_macos MACOSX_BUNDLE src/app/launcher_main.cpp)
target_link_libraries(daytona_macos PRIVATE runtime ${M2_SDL_TARGET})
set_target_properties(daytona_macos PROPERTIES
    OUTPUT_NAME daytona
    RUNTIME_OUTPUT_DIRECTORY "${_m2_app_output_dir}"
    MACOSX_BUNDLE_BUNDLE_NAME "Daytona USA"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.boucydesigns.daytona"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/macos/Info.plist.in"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

set(_m2_bundle_deps daytona_macos)
set(_m2_deluxe_game "$<TARGET_BUNDLE_DIR:daytona_macos>/../daytona-93")
set(_m2_revision_game "$<TARGET_BUNDLE_DIR:daytona_macos>/../daytona-rev")
if(TARGET daytona_game)
    list(APPEND _m2_bundle_deps daytona_game)
    if(M2_ROMSET STREQUAL "daytona93")
        set(_m2_deluxe_game "$<TARGET_FILE:daytona_game>")
    else()
        set(_m2_revision_game "$<TARGET_FILE:daytona_game>")
    endif()
endif()

find_program(M2_CODESIGN_EXECUTABLE codesign)
# A POST_BUILD on the launcher would miss independently rebuilt games.
add_custom_target(daytona_bundle ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DM2_BUNDLE_DIR=$<TARGET_BUNDLE_DIR:daytona_macos>"
        "-DM2_DELUXE_GAME=${_m2_deluxe_game}"
        "-DM2_REVISION_GAME=${_m2_revision_game}"
        "-DM2_CODESIGN=${M2_CODESIGN_EXECUTABLE}"
        -P "${CMAKE_SOURCE_DIR}/cmake/package_macos_app.cmake"
    DEPENDS ${_m2_bundle_deps}
    COMMENT "Packaging daytona.app"
    VERBATIM)

