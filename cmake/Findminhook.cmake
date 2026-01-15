if(TARGET minhook)
    set(minhook_FOUND TRUE)
    return()
endif()

if(NOT
   DEFINED
   minhook-detours_SOURCE_DIR
   OR minhook-detours_SOURCE_DIR
      STREQUAL
      ""
   )
    message(FATAL_ERROR "Findminhook.cmake: 'minhook-detours_SOURCE_DIR' is not set. "
                        "Set it to the MinHook-Detours source directory before calling find_package(minhook)."
            )
endif()

set(_minhook_root "${minhook-detours_SOURCE_DIR}")

file(
    GLOB_RECURSE
    _minhook_sources
    CONFIGURE_DEPENDS
    "${_minhook_root}/*.c"
    "${_minhook_root}/*.cc"
    "${_minhook_root}/*.cpp"
    )
file(
    GLOB_RECURSE
    _minhook_headers
    CONFIGURE_DEPENDS
    "${_minhook_root}/*.h"
    "${_minhook_root}/*.hpp"
    )

if(_minhook_sources
   STREQUAL
   ""
   )
    message(FATAL_ERROR "Findminhook.cmake: No MinHook sources found under '${_minhook_root}'.")
endif()

add_library(minhook STATIC ${_minhook_sources} ${_minhook_headers})

target_include_directories(minhook PUBLIC "${_minhook_root}/src")

target_include_directories(
    minhook
    PRIVATE "${_minhook_root}/src"
            "${_minhook_root}/src/phnt"
            "${_minhook_root}/src/SlimDetours"
    )

target_link_libraries(minhook PUBLIC ntdll.lib)

set(minhook_FOUND TRUE)
set(minhook_INCLUDE_DIRS "${_minhook_root}/src")
set(minhook_LIBRARIES minhook)
