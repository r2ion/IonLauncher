cmake_minimum_required(VERSION 3.15)
cmake_policy(
    SET
    CMP0091
    NEW
    )

project(
    Northstar
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

include(config.cmake OPTIONAL)

# Language specs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VS_PLATFORM_TOOLSET v143)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
# Deal with MSVC incompatiblity
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

# This determines the real binary root directory
if(NOT
   DEFINED
   NS_BINARY_DIR
   )
    set(NS_BINARY_DIR ${CMAKE_BINARY_DIR}/game)
endif()

# NOTE [Fifty]: Visual studio deems Build root with the value "${projectDir}" in CMakeSettings.json as invalid and
# defaults to using a temporary dir somewhere in %USER%/CMakeBuilds. To combat this we set it to "${projectDir}/build"
# and then link binaries in ${CMAKE_BINARY_DIR}/game. This means you can copy your game into ${CMAKE_BINARY_DIR}/game
# without it being cluttered up by cmake files.

message(STATUS "NS: Building to ${NS_BINARY_DIR}")

list(
    APPEND
    CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    )

include(CPM)
include(utils)

add_library(NorthstarIncludes INTERFACE)
target_include_directories(NorthstarIncludes INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB_RECURSE NS_PUBLIC_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
target_sources(NorthstarIncludes INTERFACE "$<BUILD_INTERFACE:${NS_PUBLIC_HEADERS}>")

set(BUILD_SHARED_LIBS OFF)

set(SPDLOG_WCHAR_SUPPORT ON)

if (NOT DEFINED CPM_SOURCE_CACHE)
	set(CPM_SOURCE_CACHE "${CMAKE_BINARY_DIR}/.cpm_cache")
endif()

set(RAPIDJSON_BUILD_TESTS OFF)
set(RAPIDJSON_BUILD_EXAMPLES OFF)
set(RAPIDJSON_BUILD_DOC OFF)
set(RAPIDJSON_BUILD_THIRDPARTY_GTEST OFF)

set(CURL_USE_LIBPSL OFF)
set(BUILD_CURL_EXE OFF)
set(CURL_USE_SCHANNEL ON)
set(CMAKE_USE_OPENSSL OFF)
set(USE_LIBIDN2 OFF)
set(USE_LIBPSL OFF)
set(USE_LIBSSH2 OFF)
set(ENABLE_ARES OFF)
set(CURL_ZLIB OFF)
set(CURL_DISABLE_LDAP ON)
set(CURL_DISABLE_LDAPS ON)

CPMAddPackage("gh:gabime/spdlog@1.17.0")
CPMAddPackage("gh:Tencent/rapidjson@1.1.0")

CPMAddPackage(
  	NAME libcurl
  	GITHUB_REPOSITORY curl/curl
  	GIT_TAG curl-8_18_0
)

add_subdirectory(src)
add_subdirectory(proxy)
add_subdirectory(launcher)

# Forces Minizip to not use functions that are not available in Win 7 libraries.
if(WIN32)
    target_compile_definitions(minizip PUBLIC -D_WIN32_WINNT=0x0601)
endif()
