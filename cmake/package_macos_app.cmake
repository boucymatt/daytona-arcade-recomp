set(_m2_executable_dir "${M2_BUNDLE_DIR}/Contents/MacOS")
file(MAKE_DIRECTORY "${_m2_executable_dir}")
foreach(_m2_set IN ITEMS DELUXE REVISION)
    if(_m2_set STREQUAL "DELUXE")
        set(_m2_name daytona-93)
    else()
        set(_m2_name daytona-rev)
    endif()
    if(EXISTS "${M2_${_m2_set}_GAME}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${M2_${_m2_set}_GAME}" "${_m2_executable_dir}/${_m2_name}"
            COMMAND_ERROR_IS_FATAL ANY)
        if(M2_CODESIGN AND EXISTS "${M2_CODESIGN}")
            execute_process(COMMAND "${M2_CODESIGN}" --force --sign -
                "${_m2_executable_dir}/${_m2_name}"
                COMMAND_ERROR_IS_FATAL ANY)
        endif()
    else()
        file(REMOVE "${_m2_executable_dir}/${_m2_name}")
    endif()
endforeach()

# Local builds need no developer identity. Sign after copying the games so
# the app's resource seal describes the completed bundle.
if(M2_CODESIGN AND EXISTS "${M2_CODESIGN}")
    execute_process(COMMAND "${M2_CODESIGN}" --force --sign - "${M2_BUNDLE_DIR}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()

