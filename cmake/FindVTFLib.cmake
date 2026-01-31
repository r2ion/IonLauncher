cmake_minimum_required(VERSION 3.15)

# FindVTFLib
#
# Variables:
#   VTFLib_FOUND
#   VTFLib_INCLUDE_DIR
#   VTFLib_LIBRARY
#   VTFLib_DLL (optional)
#
# Imported target:
#   VTFLib::VTFLib

include(FindPackageHandleStandardArgs)

set(_VTFLib_hint_roots "")

if(DEFINED VTFLIB_ROOT)
    list(APPEND _VTFLib_hint_roots "${VTFLIB_ROOT}")
endif()

if(DEFINED VTFEDIT_RELOADED_ROOT)
    list(APPEND _VTFLib_hint_roots "${VTFEDIT_RELOADED_ROOT}")
endif()

if(DEFINED VTFEDIT_RELOADED_SOURCE_DIR)
    list(APPEND _VTFLib_hint_roots "${VTFEDIT_RELOADED_SOURCE_DIR}")
endif()

if(DEFINED vtfedit-reloaded_SOURCE_DIR)
    list(APPEND _VTFLib_hint_roots "${vtfedit-reloaded_SOURCE_DIR}")
endif()

if(DEFINED vtfedit-reloaded_BINARY_DIR)
    list(APPEND _VTFLib_hint_roots "${vtfedit-reloaded_BINARY_DIR}")
endif()

if(DEFINED VTFLib_ROOT)
    list(APPEND _VTFLib_hint_roots "${VTFLib_ROOT}")
endif()

# Common CPM/FetchContent layouts (in-source and out-of-source builds)
if(DEFINED CMAKE_BINARY_DIR)
    list(APPEND _VTFLib_hint_roots
        "${CMAKE_BINARY_DIR}/_deps/vtfedit-reloaded-src"
        "${CMAKE_BINARY_DIR}/_deps/vtfedit-reloaded-build"
    )
endif()

if(DEFINED CMAKE_SOURCE_DIR)
    list(APPEND _VTFLib_hint_roots
        "${CMAKE_SOURCE_DIR}/build/_deps/vtfedit-reloaded-src"
        "${CMAKE_SOURCE_DIR}/build/_deps/vtfedit-reloaded-build"
    )
endif()

list(REMOVE_DUPLICATES _VTFLib_hint_roots)

find_path(
    VTFLib_INCLUDE_DIR
    NAMES VTFLib.h
    HINTS ${_VTFLib_hint_roots}
    PATH_SUFFIXES
        VTFLib
    include
    lib
)

find_path(
    VTFLib_SOURCE_DIR
    NAMES VTFLib.cpp
    HINTS ${_VTFLib_hint_roots}
    PATH_SUFFIXES
        VTFLib
)

find_path(
    VTFLib_THIRDPARTY_INCLUDE_DIR
    NAMES Compressonator.h
    HINTS ${_VTFLib_hint_roots}
    PATH_SUFFIXES
        thirdparty/include
        include
)

find_library(
    VTFLib_LIBRARY
    NAMES VTFLib VTFLibd
    HINTS ${_VTFLib_hint_roots}
    PATH_SUFFIXES
        lib
        lib/x64
        lib/Release
        lib/Debug
        x64/Release
        x64/Debug
        Release
        Debug
)

find_library(
    VTFLib_COMPRESSONATOR_LIBRARY_RELEASE
    NAMES Compressonator_MT
    HINTS ${_VTFLib_hint_roots}
    PATH_SUFFIXES
        thirdparty/lib
        lib
)

find_library(
    VTFLib_COMPRESSONATOR_LIBRARY_DEBUG
    NAMES Compressonator_MTd
    HINTS ${_VTFLib_hint_roots}
    PATH_SUFFIXES
        thirdparty/lib
        lib
)

if(VTFLib_LIBRARY)
    get_filename_component(_VTFLib_lib_dir "${VTFLib_LIBRARY}" DIRECTORY)
    find_file(
        VTFLib_DLL
        NAMES VTFLib.dll VTFLibd.dll
        HINTS "${_VTFLib_lib_dir}" ${_VTFLib_hint_roots}
        PATH_SUFFIXES
            bin
            bin/x64
            bin/Release
            bin/Debug
    )
endif()

if(VTFLib_LIBRARY)
    find_package_handle_standard_args(
        VTFLib
        REQUIRED_VARS VTFLib_INCLUDE_DIR VTFLib_LIBRARY
    )

    if(VTFLib_FOUND AND NOT TARGET VTFLib::VTFLib)
        add_library(VTFLib::VTFLib UNKNOWN IMPORTED)
        set_target_properties(
            VTFLib::VTFLib
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${VTFLib_INCLUDE_DIR}"
                IMPORTED_LOCATION "${VTFLib_LIBRARY}"
        )

        if(VTFLib_THIRDPARTY_INCLUDE_DIR)
            set_property(
                TARGET VTFLib::VTFLib
                APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES "${VTFLib_THIRDPARTY_INCLUDE_DIR}"
            )
        endif()

        if(VTFLib_COMPRESSONATOR_LIBRARY_RELEASE OR VTFLib_COMPRESSONATOR_LIBRARY_DEBUG)
            set_property(
                TARGET VTFLib::VTFLib
                APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES
                        "$<$<CONFIG:Debug>:${VTFLib_COMPRESSONATOR_LIBRARY_DEBUG}>"
                        "$<$<NOT:$<CONFIG:Debug>>:${VTFLib_COMPRESSONATOR_LIBRARY_RELEASE}>"
            )
        endif()

        if(VTFLib_DLL)
            set_target_properties(
                VTFLib::VTFLib
                PROPERTIES
                    IMPORTED_IMPLIB "${VTFLib_LIBRARY}"
                    IMPORTED_LOCATION "${VTFLib_DLL}"
            )
        endif()
    endif()
elseif(VTFLib_SOURCE_DIR)
    # Build VTFLib from source (as shipped in vtfedit-reloaded)
    set(VTFLib_INCLUDE_DIR "${VTFLib_SOURCE_DIR}")

    if(NOT VTFLib_THIRDPARTY_INCLUDE_DIR)
        get_filename_component(_VTFLib_root "${VTFLib_SOURCE_DIR}" DIRECTORY)
        set(_VTFLib_thirdparty_candidate "${_VTFLib_root}/thirdparty/include")
        if(EXISTS "${_VTFLib_thirdparty_candidate}")
            set(VTFLib_THIRDPARTY_INCLUDE_DIR "${_VTFLib_thirdparty_candidate}")
        endif()
    endif()

    if(NOT TARGET VTFLib::VTFLib)
        file(GLOB VTFLib_SOURCES CONFIGURE_DEPENDS
            "${VTFLib_SOURCE_DIR}/*.cpp"
            "${VTFLib_SOURCE_DIR}/*.rc"
        )

        add_library(VTFLib SHARED ${VTFLib_SOURCES})
        add_library(VTFLib::VTFLib ALIAS VTFLib)

        target_include_directories(VTFLib PUBLIC "${VTFLib_SOURCE_DIR}")
        if(VTFLib_THIRDPARTY_INCLUDE_DIR)
            target_include_directories(VTFLib PUBLIC "${VTFLib_THIRDPARTY_INCLUDE_DIR}")
        endif()
        if(VTFLib_COMPRESSONATOR_LIBRARY_RELEASE OR VTFLib_COMPRESSONATOR_LIBRARY_DEBUG)
            target_link_libraries(
                VTFLib
                PRIVATE
                    $<$<CONFIG:Debug>:${VTFLib_COMPRESSONATOR_LIBRARY_DEBUG}>
                    $<$<NOT:$<CONFIG:Debug>>:${VTFLib_COMPRESSONATOR_LIBRARY_RELEASE}>
            )
        endif()
        target_compile_definitions(VTFLib PRIVATE VTFLIB_EXPORTS)
    endif()

    set(VTFLib_FOUND TRUE)
endif()

mark_as_advanced(
    VTFLib_INCLUDE_DIR
    VTFLib_LIBRARY
    VTFLib_DLL
    VTFLib_SOURCE_DIR
    VTFLib_THIRDPARTY_INCLUDE_DIR
    VTFLib_COMPRESSONATOR_LIBRARY_RELEASE
    VTFLib_COMPRESSONATOR_LIBRARY_DEBUG
)
