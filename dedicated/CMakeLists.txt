# NorthstarLauncher

add_executable(NorthstarLauncherCXX WIN32 "main.cpp" "resources.rc")
add_executable(r2ds "main.cpp" "resources.rc")

set(launcher_targets NorthstarLauncherCXX r2ds)
set(launcher_libs
    shlwapi.lib
    kernel32.lib
    user32.lib
    gdi32.lib
    winspool.lib
    comdlg32.lib
    advapi32.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    odbc32.lib
    odbccp32.lib
    ws2_32.lib
    )

foreach(target ${launcher_targets})
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
    target_link_libraries(${target} PRIVATE ${launcher_libs})
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    set_target_properties(
        ${target}
        PROPERTIES LINK_FLAGS "/MANIFEST:NO /DEBUG /STACK:8000000"
                   RUNTIME_OUTPUT_DIRECTORY "${NS_BINARY_DIR}"
                   RUNTIME_OUTPUT_DIRECTORY_DEBUG "${NS_BINARY_DIR}"
                   RUNTIME_OUTPUT_DIRECTORY_RELEASE "${NS_BINARY_DIR}"
                   RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${NS_BINARY_DIR}"
                   RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${NS_BINARY_DIR}"
        )
endforeach()

target_compile_definitions(r2ds PRIVATE DEDICATED)
