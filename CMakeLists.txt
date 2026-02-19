cmake_minimum_required(VERSION 4.1.1)

cmake_policy(
    SET
    CMP0091
    NEW
    )
cmake_policy(
    SET
    CMP0169
    OLD
    )
cmake_policy(
    SET
    CMP0175
    OLD
    )

project(
    Northstar
    C
	CSharp
    CXX
    ASM_MASM
    )

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING
              "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
              FORCE
        )
endif()

# Language specs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
# Deal with MSVC incompatiblity
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

# This determines the real binary root directory
set(NS_BINARY_DIR_DEFAULT "${CMAKE_BINARY_DIR}/game")
set(NS_PUBLISH_LAUNCHER_DEFAULT OFF)
set(NS_LAUNCHER_RUNTIME_IDENTIFIER_DEFAULT "win-x64")
set(NS_BUILD_LEGACY_CPP_LAUNCHER_DEFAULT ON) # temp for now, C# launcher isn't finished and kind of janky
set(NS_GENERATE_VS_LAUNCH_CONFIG_DEFAULT ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/config.cmake")
else()
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/config.cmake"
        @ONLY
        )
    include("${CMAKE_CURRENT_SOURCE_DIR}/config.cmake" OPTIONAL)
endif()

if(NOT
   DEFINED
   NS_BINARY_DIR
   )
    set(NS_BINARY_DIR "${NS_BINARY_DIR_DEFAULT}")
endif()

option(
    NS_PUBLISH_LAUNCHER
    "Publish C# launcher as a self-contained single-file executable during build"
    ${NS_PUBLISH_LAUNCHER_DEFAULT}
    )
option(
    NS_BUILD_LEGACY_CPP_LAUNCHER
    "Build legacy C++ launcher executable (NorthstarLauncherCXX)"
    ${NS_BUILD_LEGACY_CPP_LAUNCHER_DEFAULT}
    )
option(
	NS_GENERATE_VS_LAUNCH_CONFIG
	"Generate launch.vs.json for Visual Studio debugging configuration"
	${NS_GENERATE_VS_LAUNCH_CONFIG_DEFAULT}
	)
set(
    NS_LAUNCHER_RUNTIME_IDENTIFIER
    "${NS_LAUNCHER_RUNTIME_IDENTIFIER_DEFAULT}"
    CACHE STRING
          "Runtime identifier used when publishing the C# launcher."
    )

set(NS_VERSION_NUMERIC
    "0,0,0,1"
    CACHE STRING
          "Windows numeric FILEVERSION/PRODUCTVERSION value (comma separated)."
    )
set(NS_VERSION_STRING
    "DEV"
    CACHE STRING
          "Windows string FileVersion/ProductVersion value."
    )
set(NS_ION_PATCH
    "0"
    CACHE STRING
          "ION patch value for include/ns_version.h."
    )

add_compile_definitions(
    NORTHSTAR_VERSION=${NS_VERSION_NUMERIC}
    NORTHSTAR_VERSION_STR="${NS_VERSION_STRING}"
    ION_PATCH=${NS_ION_PATCH}
    )

# NOTE [Fifty]: Visual studio deems Build root with the value "${projectDir}" in CMakeSettings.json as invalid and
# defaults to using a temporary dir somewhere in %USER%/CMakeBuilds. To combat this we set it to "${projectDir}/build"
# and then link binaries in ${CMAKE_BINARY_DIR}/game. This means you can copy your game into ${CMAKE_BINARY_DIR}/game
# without it being cluttered up by cmake files.

message(STATUS "NS: Building to ${NS_BINARY_DIR}")
message(STATUS "NS: Publish launcher ${NS_PUBLISH_LAUNCHER} (RID: ${NS_LAUNCHER_RUNTIME_IDENTIFIER})")
message(STATUS "NS: Build legacy C++ launcher ${NS_BUILD_LEGACY_CPP_LAUNCHER}")

list(
    APPEND
    CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    )

include(CPM)

add_library(NorthstarIncludes INTERFACE)
target_include_directories(NorthstarIncludes INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(
    GLOB_RECURSE
    NS_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    )
target_sources(NorthstarIncludes INTERFACE "$<BUILD_INTERFACE:${NS_PUBLIC_HEADERS}>")

target_precompile_headers(
    NorthstarIncludes
    INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/pch.h>"
    )

set(BUILD_SHARED_LIBS OFF)

set(SPDLOG_WCHAR_SUPPORT ON)

if(NOT
   DEFINED
   CPM_SOURCE_CACHE
   )
    set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/.cpm_cache")
endif()

set(RAPIDJSON_BUILD_TESTS OFF)
set(RAPIDJSON_BUILD_EXAMPLES OFF)
set(RAPIDJSON_BUILD_DOC OFF)
set(RAPIDJSON_BUILD_THIRDPARTY_GTEST OFF)

set(CURL_USE_LIBPSL OFF)
set(BUILD_CURL_EXE OFF)
set(BUILD_EXAMPLES OFF)
set(CURL_USE_SCHANNEL ON)
set(CMAKE_USE_OPENSSL OFF)
set(USE_LIBIDN2 OFF)
set(USE_LIBPSL OFF)
set(USE_LIBSSH2 OFF)
set(ENABLE_ARES OFF)
set(CURL_ZLIB OFF)
set(CURL_DISABLE_LDAP ON)
set(CURL_DISABLE_LDAPS ON)
set(CURL_BUILD_TESTING OFF)
set(BUILD_LIBCURL_DOCS OFF)
set(CURL_ENABLE_EXPORT_TARGET OFF)

set(MZ_FETCH_LIBS ON)
set(MZ_FORCE_FETCH_LIBS ON)
set(MZ_ZLIB ON)
set(MZ_ZLIB_FLAVOR zlib-ng)
set(MZ_INSTALL OFF)
set(WITH_GTEST OFF)
set(BUILD_TESTING OFF)
set(XZ_TOOL_XZ OFF)
set(XZ_TOOL_XZDEC OFF)
set(XZ_TOOL_LZMADEC OFF)
set(XZ_TOOL_LZMAINFO OFF)
set(XZ_TOOL_SCRIPTS OFF)
set(XZ_NLS OFF)
set(XZ_DOXYGEN OFF)
set(XZ_DOC OFF)
# I forgot how to add functions to cmake-format settings :(
# cmake-format: off
CPMAddPackage("gh:gabime/spdlog@1.17.0")
CPMAddPackage("gh:facebook/zstd@1.5.7")

set(RAPIDJSON_GIT_TAG "24b5e7a8b27f42fa16b96fc70aade9106cf7102f" CACHE STRING "rapidjson git tag/branch/commit to fetch")

# Use configured rapidjson ref for modern CMake compatibility
CPMAddPackage(
    NAME rapidjson
    GITHUB_REPOSITORY Tencent/rapidjson
    GIT_TAG ${RAPIDJSON_GIT_TAG}
)
CPMAddPackage("gh:m417z/minhook-detours@1.0.6")

CPMAddPackage(
	NAME minizip-ng
	GITHUB_REPOSITORY zlib-ng/minizip-ng
	GIT_TAG 4.0.10
	OPTIONS
        "SKIP_INSTALL_ALL ON"
)

CPMAddPackage(
  	NAME libcurl
  	GITHUB_REPOSITORY curl/curl
  	GIT_TAG curl-8_18_0
)

CPMAddPackage(
  	NAME vtfedit-reloaded
  	GITHUB_REPOSITORY Sky-rym/VTFEdit-Reloaded
  	GIT_TAG 2.1.9
)

# cmake-format: on

add_subdirectory(src)
add_subdirectory(proxy)
add_subdirectory(dedicated)
# add_subdirectory(launcher)

# Forces Minizip to not use functions that are not available in Win 7 libraries.
if(WIN32)
    target_compile_definitions(minizip PUBLIC -D_WIN32_WINNT=0x0601)
endif()
