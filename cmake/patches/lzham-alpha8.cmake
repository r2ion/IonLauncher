set(patch_file "${CMAKE_CURRENT_LIST_DIR}/lzham-alpha8-stdint.patch")
execute_process(
    COMMAND git apply --ignore-whitespace "${patch_file}"
    WORKING_DIRECTORY "${LZHAM_SOURCE_DIR}"
    RESULT_VARIABLE patch_result
    ERROR_VARIABLE patch_error
)
if(NOT patch_result EQUAL 0)
    execute_process(
        COMMAND git apply --ignore-whitespace --reverse --check "${patch_file}"
        WORKING_DIRECTORY "${LZHAM_SOURCE_DIR}"
        RESULT_VARIABLE already_applied
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT already_applied EQUAL 0)
        message(FATAL_ERROR "Cannot apply the LZHAM Alpha8 compatibility patch: ${patch_error}")
    endif()
endif()
