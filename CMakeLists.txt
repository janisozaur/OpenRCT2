# CMAKE project for openrct2
cmake_minimum_required(VERSION 3.24)

if (CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

if (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12")
	message(FATAL_ERROR "GCC 12+ required. Older GCC are known to fail compilation")
endif()

# Note: Searching for CCache must be before project() so project() can use CCache too
# if it is available
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(OPENRCT2_USE_CCACHE "Use CCache to improve recompilation speed (optional)" ON)

if (OPENRCT2_USE_CCACHE)
    find_package(CCache)

    if (CCache_FOUND)
         # Use e.g. "ccache clang++" instead of "clang++"
         set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCache_EXECUTABLE}")
         set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK    "${CCache_EXECUTABLE}")
    else()
        message("Usage of CCache was enabled, but CCache was not found, so CCache is not being enabled.")
    endif()
endif (OPENRCT2_USE_CCACHE)

if (APPLE)
    execute_process(COMMAND /usr/bin/uname -m OUTPUT_VARIABLE SYSTEM_MACOS_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT DEFINED ARCH)
        set(ARCH "${SYSTEM_MACOS_ARCH}")
    endif ()

    set(CMAKE_OSX_ARCHITECTURES "${ARCH}" CACHE STRING "")

    if("${ARCH}" MATCHES "^x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "")
    elseif("${ARCH}" MATCHES "^arm64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
    else()
        message("Unknown macOS architecture: ${ARCH}. Behavior may be unexpected.")
    endif()
endif ()

if (APPLE)
    set(CMAKE_SYSTEM_PROCESSOR "${CMAKE_OSX_ARCHITECTURES}" CACHE STRING "")
endif ()

# C Language required for quickjs-ng
project(openrct2 CXX C)

include(cmake/platform.cmake)
include(CMakeDependentOption)

# vcpkg includes its own copy of *.cmake; only include ours as needed.
if (NOT MSVC)
    include(FindPkgConfig)
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
endif ()
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets.json" ASSETS_JSON)

string(JSON TITLE_SEQUENCE_URL GET "${ASSETS_JSON}" "title-sequences" url)
string(JSON TITLE_SEQUENCE_SHA256 GET "${ASSETS_JSON}" "title-sequences" sha256)
get_filename_component(TITLE_SEQUENCE_ZIPNAME ${TITLE_SEQUENCE_URL} NAME)

string(JSON OBJECTS_URL GET "${ASSETS_JSON}" objects url)
string(JSON OBJECTS_SHA256 GET "${ASSETS_JSON}" objects sha256)
get_filename_component(OBJECTS_ZIPNAME ${OBJECTS_URL} NAME)

string(JSON OPENSFX_URL GET "${ASSETS_JSON}" opensfx url)
string(JSON OPENSFX_SHA256 GET "${ASSETS_JSON}" opensfx sha256)
get_filename_component(OPENSFX_ZIPNAME ${OPENSFX_URL} NAME)

string(JSON OPENMUSIC_URL GET "${ASSETS_JSON}" openmusic url)
string(JSON OPENMUSIC_SHA256 GET "${ASSETS_JSON}" openmusic sha256)
get_filename_component(OPENMUSIC_ZIPNAME ${OPENMUSIC_URL} NAME)

string(JSON REPLAYS_URL GET "${ASSETS_JSON}" replays url)
string(JSON REPLAYS_SHA256 GET "${ASSETS_JSON}" replays sha256)
get_filename_component(REPLAYS_ZIPNAME ${REPLAYS_URL} NAME)

option(FORCE32 "Force 32-bit build. It will add `-m32` to compiler flags.")
option(WITH_TESTS "Build tests")
option(PORTABLE "Create a portable build (-rpath=$ORIGIN)" OFF)
option(APPIMAGE "Create an appimage build (-rpath=$ORIGIN/../lib)" OFF)
option(DOWNLOAD_TITLE_SEQUENCES "Download title sequences during installation." ON)
option(DOWNLOAD_OBJECTS "Download objects during installation." ON)
option(DOWNLOAD_OPENSFX "Download OpenSoundEffects during installation." ON)
option(DOWNLOAD_OPENMUSIC "Download OpenMusic during installation." ON)
CMAKE_DEPENDENT_OPTION(DOWNLOAD_REPLAYS "Download replays during installation." ON
    "WITH_TESTS" OFF)
CMAKE_DEPENDENT_OPTION(MACOS_USE_DEPENDENCIES "Use OpenRCT2 dependencies instead of system libraries" ON
    "APPLE" OFF)
CMAKE_DEPENDENT_OPTION(WINDOWS_USE_DEPENDENCIES "Use OpenRCT2 dependencies instead of system libraries" ON
    "MSVC" OFF)
CMAKE_DEPENDENT_OPTION(MACOS_BUNDLE "Build macOS application bundle (OpenRCT2.app)" ON
     "MACOS_USE_DEPENDENCIES; NOT DISABLE_GUI" OFF)

# Options
option(STATIC "Create a static build.")
option(USE_MMAP "Use mmap to try loading rct2's data segment into memory.")

option(DISABLE_DISCORD_RPC "Disable Discord-RPC support." OFF)
# Currently unused, disable by default.
option(DISABLE_GOOGLE_BENCHMARK "Disable Google Benchmarks support." ON)
option(DISABLE_HTTP "Disable HTTP support.")
option(DISABLE_NETWORK "Disable multiplayer functionality. Mainly for testing.")
option(DISABLE_TTF "Disable support for TTF provided by freetype2.")
option(DISABLE_VERSION_CHECKER "Disable the check for newer versions.")
option(DISABLE_FLAC     "Disable FLAC support.")
option(DISABLE_VORBIS   "Disable OGG/VORBIS support.")
option(DISABLE_OPENGL   "Disable OpenGL support.")
option(ENABLE_SCRIPTING "Enable script / plugin support." ON)
option(ENABLE_ASAN "Enable the AddressSanitizer.")
option(ENABLE_UBSAN "Enable the UndefinedBehaviourSanitizer.")
option(ENABLE_HEADERS_CHECK "Check if include directives in header files are correct. Only works with clang" OFF)
option(DISABLE_GUI "Don't build GUI. (Headless only.)")
option(ENABLE_BREAKPAD "Enable Breakpad support." OFF)


if (FORCE32)
    set(TARGET_M "-m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TARGET_M}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${TARGET_M}")
endif ()

if (PORTABLE OR WIN32)
    set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_BINDIR}")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif ()

if (APPIMAGE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif ()

if (MACOS_USE_DEPENDENCIES)
    message("Using OpenRCT2 dependencies instead of system libraries")
    # if we're building on macOS, then we need the dependencies
    include(cmake/download.cmake)

    set(MACOS_DYLIBS_VERSION "41")
    set(MACOS_DYLIBS_ZIPFILE "openrct2-libs-v${MACOS_DYLIBS_VERSION}-universal-macos-dylibs.zip")
    set(MACOS_DYLIBS_SHA256 "7a11cf259369d87edc5140a94809f0c937db010eac03f002ad20927f1122db21")
    set(MACOS_DYLIBS_DIR "${ROOT_DIR}/lib/macos")
    set(MACOS_DYLIBS_URL "https://github.com/OpenRCT2/Dependencies/releases/download/v${MACOS_DYLIBS_VERSION}/${MACOS_DYLIBS_ZIPFILE}")

    download_openrct2_zip(
        DOWNLOAD_DIR ${MACOS_DYLIBS_DIR}
        ZIP_URL ${MACOS_DYLIBS_URL}
        SHA256 ${MACOS_DYLIBS_SHA256}
    )

    set(CMAKE_MACOSX_RPATH 1)
    list(APPEND CMAKE_PREFIX_PATH "${MACOS_DYLIBS_DIR}")

    # if we're making the OpenRCT2.app bundle, rpath will be handled by fixup_bundle
    # if we're building the CLI executable, we need to do it ourselves
    if (NOT MACOS_BUNDLE)
        message("Setting rpath for macOS dylibs")
        # the RPATH to be used when installing, but only if it's not a system directory
        list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
        if("${isSystemDir}" STREQUAL "-1")
            set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
        endif("${isSystemDir}" STREQUAL "-1")

        # if the DESTDIR env var is defined, use it in the install RPATH
        if(DEFINED ENV{DESTDIR})
            get_filename_component(destdirRealPath "$ENV{DESTDIR}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
            set(CMAKE_INSTALL_RPATH "${destdirRealPath}${CMAKE_INSTALL_PREFIX}/lib")
        endif()
    endif ()
endif ()

if (WINDOWS_USE_DEPENDENCIES)
    message("Using OpenRCT2 dependencies instead of system libraries")
    include(cmake/download.cmake)

    set(WIN_LIBS_VERSION "41")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
        set(WIN_LIBS_ARCH "arm64")
        set(WIN_LIBS_SHA256 "5d171f23eee584bdc29b1186dc7a86d935a6ea58efa42b5674c196c5b581923b")
        set(WIN_PLATFORM "ARM64")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WIN_LIBS_ARCH "x64")
        set(WIN_LIBS_SHA256 "8e78f1c3c09c50cb0f509eab6845a93dd69771ff95cdf82d747568093d138f73")
        set(WIN_PLATFORM "x64")
    else ()
        set(WIN_LIBS_ARCH "x86")
        set(WIN_LIBS_SHA256 "490263b873dd02c13a043a04d26bb9837d2d378dacd8b2c8d29660fef44db3db")
        set(WIN_PLATFORM "Win32")
    endif ()

    set(WIN_LIBS_ZIPFILE "openrct2-libs-v${WIN_LIBS_VERSION}-${WIN_LIBS_ARCH}-windows-static.zip")
    set(WIN_LIBS_URL "https://github.com/OpenRCT2/Dependencies/releases/download/v${WIN_LIBS_VERSION}/${WIN_LIBS_ZIPFILE}")
    set(WIN_LIBS_DIR "${ROOT_DIR}/lib/${WIN_PLATFORM}")

    download_openrct2_zip(
        DOWNLOAD_DIR ${WIN_LIBS_DIR}
        ZIP_URL ${WIN_LIBS_URL}
        SHA256 ${WIN_LIBS_SHA256}
    )

    include_directories("${WIN_LIBS_DIR}/include")
    include_directories("${WIN_LIBS_DIR}/include/SDL2")

    # Add dependencies that are not found via find_package
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(DEBUG_SUFFIX "d")
        set(WIN_LIBS_LIB_DIR "${WIN_LIBS_DIR}/debug/lib")
    else ()
        set(DEBUG_SUFFIX "")
        set(WIN_LIBS_LIB_DIR "${WIN_LIBS_DIR}/lib")
    endif ()

    set(HAVE_DISCORD_RPC ON)
endif ()

if (WINDOWS_USE_DEPENDENCIES)
    set(HAVE_ZLIB ON)
    set(HAVE_PNG ON)
    set(HAVE_FREETYPE ON)
    set(HAVE_SDL2 ON)
    set(HAVE_LIBZIP ON)
    set(HAVE_ZSTD ON)
    set(HAVE_BZ2 ON)
    set(HAVE_FLAC ON)
    set(HAVE_OGG ON)
    set(HAVE_VORBIS ON)
    set(PNG_FOUND ON)
    set(ZLIB_FOUND ON)
    set(ZSTD_FOUND ON)
    set(SDL2_FOUND ON)
    set(FREETYPE_FOUND ON)
    set(PkgConfig_FOUND ON)

    add_library(freetype INTERFACE)
    add_library(Freetype::Freetype ALIAS freetype)
    target_include_directories(freetype INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_FREETYPE NAMES freetype${DEBUG_SUFFIX} freetype PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(freetype INTERFACE ${LIB_FREETYPE})

    add_library(discord-rpc INTERFACE)
    target_include_directories(discord-rpc INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_DISCORD NAMES discord-rpc${DEBUG_SUFFIX} discord-rpc PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(discord-rpc INTERFACE ${LIB_DISCORD})

    add_library(SDL2 INTERFACE)
    add_library(SDL2::SDL2-static ALIAS SDL2)
    target_include_directories(SDL2 INTERFACE "${WIN_LIBS_DIR}/include/SDL2")
    find_library(LIB_SDL2 NAMES SDL2-static${DEBUG_SUFFIX} SDL2-static SDL2d SDL2 PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(SDL2 INTERFACE ${LIB_SDL2} winmm imm32 version setupapi shell32 user32 gdi32 advapi32 ole32 oleaut32 uuid)

    add_library(zlib INTERFACE)
    add_library(zlib::zlib ALIAS zlib)
    target_include_directories(zlib INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_ZLIB NAMES zlib${DEBUG_SUFFIX} zlib libz PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(zlib INTERFACE ${LIB_ZLIB})

    add_library(png INTERFACE)
    add_library(png::png ALIAS png)
    add_library(PNG::PNG ALIAS png)
    add_library(PkgConfig::PNG ALIAS png)
    target_include_directories(png INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_PNG NAMES libpng16${DEBUG_SUFFIX} libpng16 libpng PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(png INTERFACE ${LIB_PNG})

    add_library(zstd INTERFACE)
    add_library(zstd::zstd ALIAS zstd)
    add_library(zstd::libzstd_static ALIAS zstd)
    add_library(PkgConfig::ZSTD ALIAS zstd)
    target_include_directories(zstd INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_ZSTD NAMES zstd${DEBUG_SUFFIX} zstd libzstd PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(zstd INTERFACE ${LIB_ZSTD})

    add_library(bz2 INTERFACE)
    target_include_directories(bz2 INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_BZ2 NAMES bz2${DEBUG_SUFFIX} bz2 libbz2 PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(bz2 INTERFACE ${LIB_BZ2})

    add_library(flac INTERFACE)
    add_library(PkgConfig::FLAC ALIAS flac)
    target_include_directories(flac INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_FLAC NAMES flac${DEBUG_SUFFIX} flac libflac PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(flac INTERFACE ${LIB_FLAC})

    add_library(ogg INTERFACE)
    add_library(PkgConfig::OGG ALIAS ogg)
    target_include_directories(ogg INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_OGG NAMES ogg${DEBUG_SUFFIX} ogg libogg PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(ogg INTERFACE ${LIB_OGG})

    add_library(vorbis INTERFACE)
    target_include_directories(vorbis INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_VORBIS NAMES vorbis${DEBUG_SUFFIX} vorbis libvorbis PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(vorbis INTERFACE ${LIB_VORBIS})

    add_library(vorbisfile INTERFACE)
    add_library(PkgConfig::VORBISFILE ALIAS vorbisfile)
    target_include_directories(vorbisfile INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_VORBISFILE NAMES vorbisfile${DEBUG_SUFFIX} vorbisfile libvorbisfile PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(vorbisfile INTERFACE ${LIB_VORBISFILE})

    add_library(libzip INTERFACE)
    add_library(PkgConfig::LIBZIP ALIAS libzip)
    target_include_directories(libzip INTERFACE "${WIN_LIBS_DIR}/include")
    find_library(LIB_ZIP NAMES libzip${DEBUG_SUFFIX} libzip zip PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(libzip INTERFACE ${LIB_ZIP})

    add_library(brotli INTERFACE)
    find_library(LIB_BROTLICOMMON NAMES brotlicommon${DEBUG_SUFFIX} brotlicommon PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    find_library(LIB_BROTLIDEC NAMES brotlidec${DEBUG_SUFFIX} brotlidec PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    find_library(LIB_BROTLIENC NAMES brotlienc${DEBUG_SUFFIX} brotlienc PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
    target_link_libraries(brotli INTERFACE ${LIB_BROTLICOMMON} ${LIB_BROTLIDEC} ${LIB_BROTLIENC})

    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "IA64" OR CMAKE_SIZEOF_VOID_P EQUAL 4)
         if (NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
            add_library(fribidi INTERFACE)
            target_link_libraries(fribidi INTERFACE "${WIN_LIBS_LIB_DIR}/fribidi.lib")
         endif ()
    endif ()

    if (ENABLE_BREAKPAD)
        add_library(breakpad INTERFACE)
        find_library(LIB_BREAKPAD NAMES libbreakpad${DEBUG_SUFFIX} libbreakpad PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
        find_library(LIB_BREAKPAD_CLIENT NAMES libbreakpad_client${DEBUG_SUFFIX} libbreakpad_client PATHS "${WIN_LIBS_LIB_DIR}" REQUIRED)
        target_link_libraries(breakpad INTERFACE ${LIB_BREAKPAD} ${LIB_BREAKPAD_CLIENT})
    endif ()

    if (WITH_TESTS)
        add_library(gtest INTERFACE)
        add_library(GTest::gtest ALIAS gtest)
        target_include_directories(gtest INTERFACE "${WIN_LIBS_DIR}/include")
        find_library(LIB_GTEST NAMES gtest${DEBUG_SUFFIX} gtest PATHS "${WIN_LIBS_LIB_DIR}" PATH_SUFFIXES manual-link REQUIRED)
        target_link_libraries(gtest INTERFACE ${LIB_GTEST})

        add_library(gtest_main INTERFACE)
        add_library(GTest::gtest_main ALIAS gtest_main)
        target_include_directories(gtest_main INTERFACE "${WIN_LIBS_DIR}/include")
        find_library(LIB_GTEST_MAIN NAMES gtest_main${DEBUG_SUFFIX} gtest_main PATHS "${WIN_LIBS_LIB_DIR}" PATH_SUFFIXES manual-link REQUIRED)
        target_link_libraries(gtest_main INTERFACE ${LIB_GTEST_MAIN} gtest)
    endif ()
elseif (MACOS_USE_DEPENDENCIES)
    set(HAVE_ZLIB ON)
    set(HAVE_PNG ON)
    set(HAVE_FREETYPE ON)
    set(HAVE_SDL2 ON)
    set(HAVE_LIBZIP ON)
    set(HAVE_ZSTD ON)
    set(HAVE_BZ2 ON)
    set(HAVE_FLAC ON)
    set(HAVE_OGG ON)
    set(HAVE_VORBIS ON)
    set(PNG_FOUND ON)
    set(ZLIB_FOUND ON)
    set(ZSTD_FOUND ON)
    set(SDL2_FOUND ON)
    set(FREETYPE_FOUND ON)

    include_directories("${MACOS_DYLIBS_DIR}/include")
    include_directories("${MACOS_DYLIBS_DIR}/include/SDL2")

    set(MACOS_LIBS_LIB_DIR "${MACOS_DYLIBS_DIR}/lib")

    add_library(freetype INTERFACE)
    add_library(freetype::freetype ALIAS freetype)
    target_include_directories(freetype INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_FREETYPE NAMES freetype PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(freetype INTERFACE ${LIB_FREETYPE})

    add_library(SDL2 INTERFACE)
    add_library(SDL2::SDL2-static ALIAS SDL2)
    target_include_directories(SDL2 INTERFACE "${MACOS_DYLIBS_DIR}/include/SDL2")
    find_library(LIB_SDL2 NAMES SDL2 PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(SDL2 INTERFACE ${LIB_SDL2})

    add_library(zlib INTERFACE)
    add_library(zlib::zlib ALIAS zlib)
    add_library(ZLIB::ZLIB ALIAS zlib)
    add_library(PkgConfig::ZLIB ALIAS zlib)
    target_include_directories(zlib INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_ZLIB NAMES z libz PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(zlib INTERFACE ${LIB_ZLIB})

    add_library(png INTERFACE)
    add_library(png::png ALIAS png)
    add_library(PNG::PNG ALIAS png)
    add_library(PkgConfig::PNG ALIAS png)
    target_include_directories(png INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_PNG NAMES png libpng PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(png INTERFACE ${LIB_PNG})

    add_library(zstd INTERFACE)
    add_library(zstd::zstd ALIAS zstd)
    add_library(PkgConfig::ZSTD ALIAS zstd)
    target_include_directories(zstd INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_ZSTD NAMES zstd libzstd PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(zstd INTERFACE ${LIB_ZSTD})

    add_library(bz2 INTERFACE)
    target_include_directories(bz2 INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_BZ2 NAMES bz2 libbz2 PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(bz2 INTERFACE ${LIB_BZ2})

    add_library(flac INTERFACE)
    add_library(PkgConfig::FLAC ALIAS flac)
    target_include_directories(flac INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_FLAC NAMES FLAC libFLAC PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(flac INTERFACE ${LIB_FLAC})

    add_library(ogg INTERFACE)
    add_library(PkgConfig::OGG ALIAS ogg)
    target_include_directories(ogg INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_OGG NAMES ogg libogg PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(ogg INTERFACE ${LIB_OGG})

    add_library(vorbis INTERFACE)
    target_include_directories(vorbis INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_VORBIS NAMES vorbis libvorbis PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(vorbis INTERFACE ${LIB_VORBIS})

    add_library(vorbisfile INTERFACE)
    add_library(PkgConfig::VORBISFILE ALIAS vorbisfile)
    target_include_directories(vorbisfile INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_VORBISFILE NAMES vorbisfile libvorbisfile PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(vorbisfile INTERFACE ${LIB_VORBISFILE})

    add_library(libzip INTERFACE)
    add_library(PkgConfig::LIBZIP ALIAS libzip)
    target_include_directories(libzip INTERFACE "${MACOS_DYLIBS_DIR}/include")
    find_library(LIB_ZIP NAMES zip libzip PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(libzip INTERFACE ${LIB_ZIP})

    if (WITH_TESTS)
        add_library(gtest INTERFACE)
        add_library(GTest::gtest ALIAS gtest)
        target_include_directories(gtest INTERFACE "${MACOS_DYLIBS_DIR}/include")
        find_library(LIB_GTEST NAMES gtest libgtest PATHS "${MACOS_LIBS_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(gtest INTERFACE ${LIB_GTEST})

        add_library(gtest_main INTERFACE)
        add_library(GTest::gtest_main ALIAS gtest_main)
        target_include_directories(gtest_main INTERFACE "${MACOS_DYLIBS_DIR}/include")
        find_library(LIB_GTEST_MAIN NAMES gtest_main libgtest_main PATHS "${MACOS_LIBS_LIB_DIR}" PATH_SUFFIXES manual-link NO_DEFAULT_PATH REQUIRED)
        target_link_libraries(gtest_main INTERFACE ${LIB_GTEST_MAIN} gtest)
    endif ()
endif ()

# LIST of supported flags, use SET_CHECK_CXX_FLAGS() to apply to target.
# Use ADD_CHECK_CXX_COMPILER_FLAG() to add to list.
set(SUPPORTED_CHECK_CXX_COMPILER_FLAGS "")

include(cmake/ipo.cmake)
list(APPEND IPO_ENABLED_BUILDS RELEASE RELWITHDEBINFO MINSIZEREL)
ipo_enable("${IPO_ENABLED_BUILDS}")

# Describe current version in terms of closest tag
execute_process(
    COMMAND git describe HEAD
    COMMAND sed -E "s/-g.+$//"
    WORKING_DIRECTORY ${ROOT_DIR}
    OUTPUT_VARIABLE OPENRCT2_VERSION_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Define current git branch
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${ROOT_DIR}
    OUTPUT_VARIABLE OPENRCT2_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Define short commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${ROOT_DIR}
    OUTPUT_VARIABLE OPENRCT2_COMMIT_SHA1_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)


if (NOT DISABLE_DISCORD_RPC)
    if (WINDOWS_USE_DEPENDENCIES)
        set(HAVE_DISCORD_RPC TRUE)
        add_definitions(-D__ENABLE_DISCORD__)
    elseif (UNIX AND NOT APPLE)
        find_package(DiscordRPC)
        if(${DISCORDRPC_FOUND})
            set(HAVE_DISCORD_RPC TRUE)
            add_definitions(-D__ENABLE_DISCORD__)
            include_directories(DISCORDRPC_PROCESS_INCLUDES)
        endif()
    elseif (MACOS_USE_DEPENDENCIES)
        find_package(discordrpc CONFIG REQUIRED)
        if(${DISCORDRPC_FOUND})
            add_definitions(-D__ENABLE_DISCORD__)
            set(HAVE_DISCORD_RPC TRUE)
            message("Building with libdiscord-rpc.dylib")
        endif()
    elseif(NOT HAVE_DISCORD_RPC AND EXISTS "${ROOT_DIR}/discord-rpc")
        # Don't build discord's examples, some of which are in C and do not honour
        # the flags we set for C++. Also we don't use the provided examples.
        set(BUILD_EXAMPLES OFF CACHE BOOL "Build example apps")
        add_subdirectory("${ROOT_DIR}/discord-rpc")
        add_definitions(-D__ENABLE_DISCORD__)
        include_directories("${ROOT_DIR}/discord-rpc/include")
        set(HAVE_DISCORD_RPC TRUE)
        message("Building with discord-rpc support")
    else()
        message("No discord-rpc detected, to enable clone discord-rpc to root directory: ${ROOT_DIR}")
    endif()
endif()

# ensure X86 and X86_64 are always initialized
# may be overwritten below
set(X86 0)
set(X86_64 0)
# Copied from https://github.com/opencv/opencv/blob/dcdd6af5a856826fe62c95322145731e702e54c5/cmake/OpenCVDetectCXXCompiler.cmake#L63-L70
if(MSVC64 OR MINGW64)
    set(X86_64 1)
elseif(MINGW OR (MSVC AND NOT CMAKE_CROSSCOMPILING))
    set(X86 1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*")
    set(X86_64 1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*|amd64.*|AMD64.*")
    set(X86 1)
elseif(CMAKE_OSX_ARCHITECTURES MATCHES "amd64.*|x86_64.*|AMD64.*")
    set(X86_64 1)
endif()

# Check if a flag exists and add it to the list of compiler (so, not linker) options
function (ADD_CHECK_CXX_COMPILER_FLAG _CXXFLAGS _CACHE_VAR _FLAG)
    CHECK_CXX_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
    if (${_CACHE_VAR})
        set(SUPPORTED_CHECK_CXX_COMPILER_FLAGS ${SUPPORTED_CHECK_CXX_COMPILER_FLAGS} ${_FLAG} PARENT_SCOPE)
    else ()
        message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
    endif ()
endfunction ()

# Add check flags to a compile TARGET
function (SET_CHECK_CXX_FLAGS _TARGET)
    target_compile_options("${_TARGET}" PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${SUPPORTED_CHECK_CXX_COMPILER_FLAGS}>)
endfunction()

# Check if a flag exists and add it to the compiler and the linker options
function (ADD_CHECK_CXX_FLAG _CXXFLAGS _CACHE_VAR _FLAG)
    CHECK_CXX_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
    if (${_CACHE_VAR})
        set(${_CXXFLAGS} "${${_CXXFLAGS}} ${_FLAG}" PARENT_SCOPE)
    else ()
        message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
    endif ()
endfunction ()

if (MSVC)
    # CMAKE does not have a built-in option for setting the CRT, so override the default flags.
    # NOTE: doing it this way avoids a linker warning about one directive overriding another
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /DNDEBUG")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /permissive- /Zc:externConstexpr /EHsc /WX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4068") # C4068: unknown pragma
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4091") # C4091: 'keyword': ignored on left of 'type' when no variable is declared
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100") # C4100: 'identifier': unreferenced formal parameter
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4121") # C4121: 'alignment of a member was sensitive to packing
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4132") # C4132: 'const object should be initialized
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4200") # C4200: nonstandard extension used: zero-sized array in struct/union
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201") # C4201: nonstandard extension used: nameless struct/union
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4204") # C4204: nonstandard extension used: non-constant aggregate initializer
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4206") # C4206: nonstandard extension used: translation unit is empty
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4221") # C4221: nonstandard extension used: 'identifier': cannot be initialized using address of automatic variable 'identifier'
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244") # C4244: 'conversion_type': conversion from 'type1' to 'type2', possible loss of data

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4263") # C4263: 'member function does not override any base class virtual member function
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4265") # C4265: 'class': class has virtual functions, but destructor is not virtual
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4548") # C4548: expression before comma has no effect
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4549") # C4549: 'operator': operator before comma has no effect
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4555") # C4555: expression has no effect

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:NOLBR /ignore:4099")

    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
    endif ()

    # Enable char8_t<->char conversion
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:char8_t-")

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    add_definitions(-DSDL_MAIN_HANDLED)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    if (X86 OR X86_64)
        add_definitions(-D__SSE4_1__)
        add_definitions(-D__AVX2__)
    endif ()
    add_definitions(-DNOMINMAX)

    if (ENABLE_BREAKPAD)
        add_definitions(-DUSE_BREAKPAD)
    endif ()
else ()
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_SUGGEST_OVERRIDE -Wsuggest-override)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_DUPLICATED_COND -Wduplicated-cond)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_NON_VIRTUAL_DTOR -Wnon-virtual-dtor)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_MISSING_VARIABLE_DECLARATIONS -Wmissing-variable-declarations)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_DUPLICATED_BRANCHES -Wduplicated-branches)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_RESTRICT -Wrestrict)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_MISSING_FIELD_INITIALIZERS -Wmissing-field-initializers)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_UNREACHABLE_CODE_BREAK -Wunreachable-code-break)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_LOGICAL_OP -Wlogical-op)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_RANGE_LOOP_ANALYSIS -Wrange-loop-analysis)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_TAUTOLOGICAL_ZERO_COMPARE -Wtautological-unsigned-zero-compare)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_OLD_STYLE_CAST -Wold-style-cast)
    # unused-const-variable=1 only checks main compilation unit
    # unused-const-variable[=2] would check for unused values from headers as well
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_UNUSED_CONST_VARIABLE -Wunused-const-variable=1)
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WNO_CLOBBERED -Wno-clobbered)
    # Disabled due to problems compiling OpenSSL on macOS.
    # ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_DOCUMENTATION -Wdocumentation)

    # Items below are not supported by ICC
    if (NOT MINGW)
        # Do not enable for MinGW, as its headers contain redundant declarations of builtin functions
        ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_REDUNDANT_DECLS -Wredundant-decls)

        # Currently used MinGW w/GCC 7.2 doesn't provide sane error messages for this
        ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_NULL_DEREFERENCE -Wnull-dereference)
    endif ()
    # These have no workarounds available when building with LTO
    if (NOT IPO_BUILD_ENABLED)
        ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_SUGGEST_FINAL_TYPES -Wsuggest-final-types)
        ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_SUGGEST_FINAL_METHODS -Wsuggest-final-methods)
    endif ()
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_IGNORED_QUALIFIERS -Wignored-qualifiers)

    # -Wstrict-overflow is only active when -fstrict-overflow is enabled, but -fstrict-overflow
    # is enabled on -O2, -O3, -Os. This should help catch bugs locally before they reach Travis
    # As of 2a435bf -Wstrict-overflow=1 passes, but higher values do not.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstrict-overflow")
    ADD_CHECK_CXX_COMPILER_FLAG(CMAKE_CXX_FLAGS CXX_WARN_STRICT_OVERFLOW -Wstrict-overflow=1)

    # Compiler flags
    set(DEBUG_LEVEL 0 CACHE STRING "Select debug level for compilation. Use value in range 0–3.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstrict-aliasing -Werror -Wundef -Wmissing-declarations -Winit-self -Wall -Wextra -Wshadow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas -Wno-missing-braces -Wno-comment -Wnonnull -Wno-unused-parameter -Wno-attributes")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=${DEBUG_LEVEL}")

    # Enable char8_t<->char conversion
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-char8_t -Wno-deprecated-declarations")

    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=objc-method-access")
    endif()

    # Clang -Wmissing-declarations differs from GCC, set -Wmissing-prototypes for parity
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-prototypes")
    endif()

    # On mingw all code is already PIC, this will avoid compiler error on redefining this option
    if (NOT MINGW)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif ()

    if (HAIKU OR (APPLE AND NOT USE_MMAP))
        set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
    else ()
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif ()
endif ()

if (CXX_WARN_SUGGEST_FINAL_TYPES)
    # Disable -Wsuggest-final-types via pragmas where due.
   add_definitions(-D__WARN_SUGGEST_FINAL_TYPES__)
endif ()

if (CXX_WARN_SUGGEST_FINAL_METHODS)
    # Disable -Wsuggest-final-methods via pragmas where due.
   add_definitions(-D__WARN_SUGGEST_FINAL_METHODS__)
endif ()

if (MINGW)
   add_definitions(-fstack-protector-strong)
endif ()

# Include sub-projects
include("${ROOT_DIR}/src/openrct2/CMakeLists.txt" NO_POLICY_SCOPE)
include("${ROOT_DIR}/src/openrct2-cli/CMakeLists.txt" NO_POLICY_SCOPE)
if(NOT DISABLE_GUI)
    include("${ROOT_DIR}/src/openrct2-ui/CMakeLists.txt" NO_POLICY_SCOPE)
endif()

# graphics files (g2.dat, fonts.dat, palettes.dat, tracks.dat)
if (NOT CMAKE_CROSSCOMPILING AND (WIN32 OR NOT WITH_TESTS))
    set(graphics_files "g2" "fonts" "palettes" "tracks")
    foreach(graphics_file ${graphics_files})
        set(output_file "${graphics_file}.dat")
        set(json_file "${ROOT_DIR}/resources/${graphics_file}/sprites.json")
        file(GLOB_RECURSE dependent_files CONFIGURE_DEPENDS "${ROOT_DIR}/resources/${graphics_file}/*")
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND $<TARGET_FILE:openrct2-cli> sprite build "${CMAKE_BINARY_DIR}/${output_file}" "${json_file}" silent
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            DEPENDS ${dependent_files} openrct2-cli
        )
        list(APPEND output_files "${output_file}")
    endforeach()

    add_custom_target(graphics ALL DEPENDS OpenRCT2::openrct2-cli ${output_files})

    if(NOT DISABLE_GUI)
        add_dependencies(openrct2 graphics)
    endif()

    if (MSVC AND NOT WITH_TESTS)
        # Copy language, shaders, and scenario patches to build directory
        add_custom_target(copy-resources ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data/language" "${CMAKE_BINARY_DIR}/data/language"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data/shaders" "${CMAKE_BINARY_DIR}/data/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data/scenario_patches" "${CMAKE_BINARY_DIR}/data/scenario_patches"
            COMMENT "Copying language, shaders, and scenario patches to build directory"
        )
        if(NOT DISABLE_GUI)
            add_dependencies(openrct2 copy-resources)
        endif()
    endif ()

    function(add_asset_download_target target_name download_dir zip_url sha256 zip_name skip_if_exists)
        set(DOWNLOAD_MARKER "${download_dir}/${zip_name}.zipversion")
        add_custom_command(
            OUTPUT "${DOWNLOAD_MARKER}"
            COMMAND ${CMAKE_COMMAND}
                -DDOWNLOAD_DIR=${download_dir}
                -DZIP_URL=${zip_url}
                -DSHA256=${sha256}
                "-DSKIP_IF_EXISTS=${skip_if_exists}"
                -P "${CMAKE_SOURCE_DIR}/cmake/download_asset.cmake"
            COMMENT "Downloading ${target_name} asset"
        )
        add_custom_target(${target_name} DEPENDS "${DOWNLOAD_MARKER}")
    endfunction()

    if (WIN32 AND NOT WITH_TESTS)
        if (DOWNLOAD_TITLE_SEQUENCES)
            add_asset_download_target(title-sequences
                "${CMAKE_BINARY_DIR}/data/sequence/"
                "${TITLE_SEQUENCE_URL}"
                "${TITLE_SEQUENCE_SHA256}"
                "${TITLE_SEQUENCE_ZIPNAME}"
                "${CMAKE_SOURCE_DIR}/data/sequence/"
            )
            if(NOT DISABLE_GUI)
                add_dependencies(openrct2 title-sequences)
            endif()
        endif ()
        if (DOWNLOAD_OBJECTS)
            add_asset_download_target(objects
                "${CMAKE_BINARY_DIR}/data/object/"
                "${OBJECTS_URL}"
                "${OBJECTS_SHA256}"
                "${OBJECTS_ZIPNAME}"
                "${CMAKE_SOURCE_DIR}/data/object/"
            )
            if(NOT DISABLE_GUI)
                add_dependencies(openrct2 objects)
            endif()
        endif ()
        if (DOWNLOAD_OPENSFX)
            add_asset_download_target(opensfx
                "${CMAKE_BINARY_DIR}/data/"
                "${OPENSFX_URL}"
                "${OPENSFX_SHA256}"
                "${OPENSFX_ZIPNAME}"
                "${CMAKE_SOURCE_DIR}/data/assetpack/openrct2.sound.parkap"
            )
            if(NOT DISABLE_GUI)
                add_dependencies(openrct2 opensfx)
            endif()
        endif ()
        if (DOWNLOAD_OPENMUSIC)
            add_asset_download_target(openmusic
                "${CMAKE_BINARY_DIR}/data/"
                "${OPENMUSIC_URL}"
                "${OPENMUSIC_SHA256}"
                "${OPENMUSIC_ZIPNAME}"
                "${CMAKE_SOURCE_DIR}/data/assetpack/openrct2.music.alternative.parkap"
            )
            if(NOT DISABLE_GUI)
                add_dependencies(openrct2 openmusic)
            endif()
        endif ()
        if (DOWNLOAD_REPLAYS)
            add_asset_download_target(replays
                "${CMAKE_BINARY_DIR}/testdata/replays/"
                "${REPLAYS_URL}"
                "${REPLAYS_SHA256}"
                "${REPLAYS_ZIPNAME}"
                ""
            )
        endif ()
    endif ()

else ()
    message("Skipping graphics generation in cross-compile")
endif ()

# Include tests
if (WITH_TESTS)
    enable_testing()
    add_subdirectory("test/tests")
endif ()

# macOS bundle "install" is handled in src/openrct2-ui/CMakeLists.txt
# This is because the openrct2 target is modified (and that is where that target is defined)
if (NOT MACOS_BUNDLE OR (MACOS_BUNDLE AND WITH_TESTS))
    # Install
    # Don't recurse, grab all *.txt and *.md files
    add_definitions(-DDOCDIR="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DOCDIR}")
    file(GLOB DOC_FILES "${ROOT_DIR}/distribution/*.txt")
    list(APPEND DOC_FILES "${ROOT_DIR}/contributors.md"
                        "${ROOT_DIR}/PRIVACY.md"
                        "${ROOT_DIR}/licence.txt"
                        "${ROOT_DIR}/distribution/scripting.md"
                        "${ROOT_DIR}/distribution/openrct2.d.ts")

    if (DOWNLOAD_TITLE_SEQUENCES)
        # Checks if this version of the title sequences are already installed, updates if necessary
        # if /data/sequence exists, it will overwrite this anyways (see `install(DIRECTORY "data/" ...)` below)
        # therefore, the existence of that directory will skip this download
        install(CODE "
            include(${ROOT_DIR}/cmake/download.cmake)
            download_openrct2_zip(
                DOWNLOAD_DIR \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/openrct2/sequence/
                SKIP_IF_EXISTS ${CMAKE_SOURCE_DIR}/data/sequence/
                ZIP_URL ${TITLE_SEQUENCE_URL}
                SHA256 ${TITLE_SEQUENCE_SHA256}
            )")
    endif ()
    if (DOWNLOAD_OBJECTS)
        # Checks if this version of the objects are already installed, updates if necessary
        # if /data/object exists, it will overwrite this anyways (see `install(DIRECTORY "data/" ...)` below)
        # therefore, the existence of that directory will skip this download
        install(CODE "
            include(${ROOT_DIR}/cmake/download.cmake)
            download_openrct2_zip(
                DOWNLOAD_DIR \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/openrct2/object/
                SKIP_IF_EXISTS ${CMAKE_SOURCE_DIR}/data/object/
                ZIP_URL ${OBJECTS_URL}
                SHA256 ${OBJECTS_SHA256}
            )")
    endif ()
    if (DOWNLOAD_OPENSFX)
        install(CODE "
            include(${ROOT_DIR}/cmake/download.cmake)
            download_openrct2_zip(
                SKIP_IF_EXISTS ${CMAKE_SOURCE_DIR}/data/assetpack/openrct2.sound.parkap
                DOWNLOAD_DIR \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/openrct2/
                ZIP_URL ${OPENSFX_URL}
                SHA256 ${OPENSFX_SHA256}
            )")
    endif ()
    if (DOWNLOAD_OPENMUSIC)
        install(CODE "
            include(${ROOT_DIR}/cmake/download.cmake)
            download_openrct2_zip(
                SKIP_IF_EXISTS ${CMAKE_SOURCE_DIR}/data/assetpack/openrct2.music.alternative.parkap
                DOWNLOAD_DIR \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/openrct2/
                ZIP_URL ${OPENMUSIC_URL}
                SHA256 ${OPENMUSIC_SHA256}
            )")
    endif ()
    if (DOWNLOAD_REPLAYS)
        # Checks if this version of the replays are already installed, updates if necessary
        install(CODE "
            include(${ROOT_DIR}/cmake/download.cmake)
            download_openrct2_zip(
                DOWNLOAD_DIR \${CMAKE_CURRENT_BINARY_DIR}/testdata/replays/
                ZIP_URL ${REPLAYS_URL}
                SHA256 ${REPLAYS_SHA256}
            )")
    endif ()
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/g2.dat" DESTINATION "${CMAKE_INSTALL_DATADIR}/openrct2")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/fonts.dat" DESTINATION "${CMAKE_INSTALL_DATADIR}/openrct2")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/palettes.dat" DESTINATION "${CMAKE_INSTALL_DATADIR}/openrct2")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tracks.dat" DESTINATION "${CMAKE_INSTALL_DATADIR}/openrct2")
    install(DIRECTORY "data/" DESTINATION "${CMAKE_INSTALL_DATADIR}/openrct2")

    # even when building WITH_TESTS, none of the below install steps are required for OpenRCT2.app
    if (NOT MACOS_BUNDLE)
        install(TARGETS "libopenrct2" LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                                    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        if(NOT DISABLE_GUI)
            install(TARGETS "openrct2" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        endif()
        install(TARGETS "openrct2-cli" OPTIONAL RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        install(FILES ${DOC_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}")
        if (NOT HAIKU)
            install(FILES "distribution/linux/io.openrct2.openrct2.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")
            if (NOT DISABLE_GUI)
                install(FILES "resources/logo/icon_x16.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x24.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/24x24/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x32.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x48.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x64.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x96.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/96x96/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_x256.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps" RENAME "openrct2.png")
                install(FILES "resources/logo/icon_flag.svg" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps" RENAME "openrct2.svg")
                install(FILES "distribution/linux/io.openrct2.openrct2.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
                install(FILES "distribution/linux/io.openrct2.savegame.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
                install(FILES "distribution/linux/io.openrct2.scenario.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
                install(FILES "distribution/linux/io.openrct2.uri.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
            endif()
            install(FILES "distribution/linux/io.openrct2.mimeinfo.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/mime/packages/" RENAME "openrct2.xml")
        endif()
        install(DIRECTORY "distribution/man/" DESTINATION "${CMAKE_INSTALL_MANDIR}/man6" FILES_MATCHING PATTERN "*.6")

        if (MACOS_USE_DEPENDENCIES)
            # Note: dependencies may have the same names as system installed libraries
            # (via homebrew). A local CMAKE_INSTALL_PREFIX is recommended to avoid issues
            file(GLOB DYLIB_FILES "${MACOS_DYLIBS_DIR}/lib/*.dylib")
            install(FILES ${DYLIB_FILES} DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        endif()
    endif()
endif()
