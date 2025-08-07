cmake_minimum_required(VERSION 3.10)

project(openrct2-android CXX)

set(CMAKE_VERBOSE_MAKEFILE on)

set(DEBUG_LEVEL 0 CACHE STRING "Select debug level for compilation. Use value in range 0–3.")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=${DEBUG_LEVEL}")

# Copied from https://github.com/opencv/opencv/blob/dcdd6af5a856826fe62c95322145731e702e54c5/cmake/OpenCVDetectCXXCompiler.cmake#L63-L70
if(MSVC64 OR MINGW64)
    set(X86_64 1)
elseif(MINGW OR (MSVC AND NOT CMAKE_CROSSCOMPILING))
    set(X86 1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*")
    set(X86_64 1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*|amd64.*|AMD64.*")
    set(X86 1)
endif()

include(ExternalProject)
ExternalProject_Add(libs
    URL https://github.com/OpenRCT2/openrct2-dependencies-android/releases/download/v13/${ANDROID_ABI}-android-dynamic.tar.zst

    SOURCE_DIR "${CMAKE_BINARY_DIR}/libs"

    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""

    BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}crypto${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}vorbis${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}vorbisfile${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}FLAC${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}brotlidec${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}brotlicommon${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}ogg${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}bz2${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}freetype${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}png16${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}SDL2${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}speexdsp${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}ssl${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icudata${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icui18n${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icuuc${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}z${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}zstd${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_STATIC_LIBRARY_PREFIX}SDL2main${CMAKE_STATIC_LIBRARY_SUFFIX}

    LOG_DOWNLOAD 1
    LOG_UPDATE 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    LOG_TEST 1
    LOG_INSTALL 1
)

# Files must be copied individually as the old Windows console does not support wildcards in the copy command
# Doing multiple individual commands looks "less bad" than one massive line
add_custom_command(TARGET libs POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
#    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/*.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libcrypto.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libvorbis.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libvorbisfile.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libFLAC.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libbrotlidec.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libbrotlicommon.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libogg.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libbz2.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libicudata.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libicuuc.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libicui18n.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libfreetype.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libpng16.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libSDL2.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libspeexdsp.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libssl.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libz.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/libs/lib/libzstd.so" ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

add_library(freetype SHARED IMPORTED)
set_target_properties(freetype PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}freetype${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(freetype libs)

add_library(png SHARED IMPORTED)
set_target_properties(png PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}png16${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(png libs)

add_library(z SHARED IMPORTED)
set_target_properties(z PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}z${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(z libs)

add_library(zstd SHARED IMPORTED)
set_target_properties(zstd PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}zstd${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(zstd libs)

add_library(SDL2 SHARED IMPORTED)
set_target_properties(SDL2 PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}SDL2${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(SDL2 libs)

add_library(SDL2main STATIC IMPORTED)
set_target_properties(SDL2main PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_STATIC_LIBRARY_PREFIX}SDL2main${CMAKE_STATIC_LIBRARY_SUFFIX}
)
add_dependencies(SDL2main libs)

add_library(speexdsp SHARED IMPORTED)
set_target_properties(speexdsp PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}speexdsp${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(speexdsp libs)

add_library(icu STATIC IMPORTED)
set_target_properties(icu PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icui18n${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(icu libs)

add_library(icuuc STATIC IMPORTED)
set_target_properties(icuuc PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icuuc${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(icuuc libs)

add_library(icudata STATIC IMPORTED)
set_target_properties(icudata PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icudata${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(icudata libs)

add_library(ssl SHARED IMPORTED)
set_target_properties(ssl PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}ssl${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(ssl libs)

add_library(crypto SHARED IMPORTED)
set_target_properties(crypto PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}crypto${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(crypto libs)

add_library(vorbis SHARED IMPORTED)
set_target_properties(vorbis PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}vorbis${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(vorbis libs)

add_library(vorbisfile SHARED IMPORTED)
set_target_properties(vorbisfile PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}vorbisfile${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(vorbisfile libs)

add_library(FLAC SHARED IMPORTED)
set_target_properties(FLAC PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}FLAC${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(FLAC libs)

add_library(brotlidec SHARED IMPORTED)
set_target_properties(brotlidec PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}brotlidec${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(brotlidec libs)

add_library(brotlicommon SHARED IMPORTED)
set_target_properties(brotlicommon PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}brotlicommon${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(brotlicommon libs)

add_library(ogg SHARED IMPORTED)
set_target_properties(ogg PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}ogg${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(ogg libs)

add_library(bz2 SHARED IMPORTED)
set_target_properties(bz2 PROPERTIES IMPORTED_LOCATION
    ${CMAKE_BINARY_DIR}/libs/lib/${CMAKE_SHARED_LIBRARY_PREFIX}bz2${CMAKE_SHARED_LIBRARY_SUFFIX}
)
add_dependencies(bz2 libs)


include_directories(SYSTEM "${CMAKE_BINARY_DIR}/libs/include")
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/libs/include/freetype")
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/libs/include/SDL2")

# now build app's shared lib
add_definitions(-DDISABLE_DISCORD_RPC -DDISABLE_OPENGL -DGL_GLEXT_PROTOTYPES -D__STDC_LIMIT_MACROS -DSDL_MAIN_HANDLED)

# Enable scripting
add_definitions(-DENABLE_SCRIPTING)

# Fix SpeexDSP compilation
add_definitions(-DHAVE_STDINT_H)

# -Werror -Wall
set(COMMON_COMPILE_OPTIONS "${COMMON_COMPILE_OPTIONS} -fstrict-aliasing -Wundef -Wmissing-declarations -Winit-self  -Wno-unknown-pragmas -Wno-unused-function -Wno-missing-braces ")
set(COMMON_COMPILE_OPTIONS "${COMMON_COMPILE_OPTIONS} -Wno-comment -Wshadow  -Wmissing-declarations -Wnonnull")
set(COMMON_COMPILE_OPTIONS "${COMMON_COMPILE_OPTIONS} -fPIC")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--undefined=Java_org_libsdl_app_SDLActivity_nativeInit")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++20 ${COMMON_COMPILE_OPTIONS} -Wnon-virtual-dtor")
get_filename_component(ORCT2_ROOT "${CMAKE_SOURCE_DIR}/../../../../../" REALPATH)

# Enable char8_t<->char conversion
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-char8_t -Wno-deprecated-declarations")

list(APPEND CMAKE_MODULE_PATH "${ORCT2_ROOT}/cmake")

find_package(CCache)

if (CCache_FOUND)
    option(OPENRCT2_USE_CCACHE "Use CCache to improve recompilation speed (optional)" ON)
    if (OPENRCT2_USE_CCACHE)
        # Use e.g. "ccache clang++" instead of "clang++"
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCache_EXECUTABLE}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK    "${CCache_EXECUTABLE}")
    endif (OPENRCT2_USE_CCACHE)
endif (CCache_FOUND)

file(GLOB_RECURSE LIBOPENRCT2_SOURCES
    "${ORCT2_ROOT}/src/thirdparty/duktape/duktape.cpp"
    "${ORCT2_ROOT}/src/openrct2/*.cpp"
    "${ORCT2_ROOT}/src/openrct2/*.h"
    "${ORCT2_ROOT}/src/openrct2/*.hpp")

file(GLOB_RECURSE OPENRCT2_GUI_SOURCES
    "${ORCT2_ROOT}/src/openrct2-ui/*.cpp"
    "${ORCT2_ROOT}/src/openrct2-ui/*.h"
    "${ORCT2_ROOT}/src/openrct2-ui/*.hpp")

if(X86 OR X86_64)
    set_source_files_properties(${ORCT2_ROOT}/src/openrct2/drawing/SSE41Drawing.cpp PROPERTIES COMPILE_FLAGS -msse4.1)
    set_source_files_properties(${ORCT2_ROOT}/src/openrct2/drawing/AVX2Drawing.cpp PROPERTIES COMPILE_FLAGS -mavx2)
endif()

# Asset versions (matching main CMakeLists.txt)
set(TITLE_SEQUENCE_VERSION "0.4.14")
set(OBJECTS_VERSION "1.7.2")
set(OPENSFX_VERSION "1.0.6")
set(OPENMSX_VERSION "1.6.1")

# Asset URLs
set(TITLE_SEQUENCE_URL "https://github.com/OpenRCT2/title-sequences/releases/download/v${TITLE_SEQUENCE_VERSION}/title-sequences.zip")
set(OBJECTS_URL "https://github.com/OpenRCT2/objects/releases/download/v${OBJECTS_VERSION}/objects.zip")
set(OPENSFX_URL "https://github.com/OpenRCT2/OpenSoundEffects/releases/download/v${OPENSFX_VERSION}/opensound.zip")
set(OPENMSX_URL "https://github.com/OpenRCT2/OpenMusic/releases/download/v${OPENMSX_VERSION}/openmusic.zip")
set(GXC_TOOLS_URL "https://github.com/IntelOrca/libsawyer/releases/download/v1.3.0/libsawyer-tools-linux-x64.tar.gz")

# Asset SHA256 checksums
set(TITLE_SEQUENCE_SHA256 "140df714e806fed411cc49763e7f16b0fcf2a487a57001d1e50fce8f9148a9f3")
set(OBJECTS_SHA256 "b4286f7af68a61b0f7fc6e379ba37bf6796286d73e97c967958bf374e3f98f35")
set(OPENSFX_SHA256 "06b90f3e19c216752df441d551b26a9e3e1ba7755bdd2102504b73bf993608be")
set(OPENMSX_SHA256 "994b350d3b180ee1cb9619fe27f7ebae3a1a5232840c4bd47a89f33fa89de1a1")

# Directories
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/openrct2")
set(TEMP_DIR "${CMAKE_BINARY_DIR}/temp")
set(GXC_TOOLS_DIR "${CMAKE_BINARY_DIR}/gxc-tools")

# Download and extract gxc tools
ExternalProject_Add(gxc-tools
    URL ${GXC_TOOLS_URL}
    SOURCE_DIR "${GXC_TOOLS_DIR}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
)

# Download title sequences
ExternalProject_Add(title-sequences
    URL ${TITLE_SEQUENCE_URL}
    URL_HASH SHA256=${TITLE_SEQUENCE_SHA256}
    SOURCE_DIR "${TEMP_DIR}/title-sequences"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
)

# Download objects
ExternalProject_Add(objects
    URL ${OBJECTS_URL}
    URL_HASH SHA256=${OBJECTS_SHA256}
    SOURCE_DIR "${TEMP_DIR}/objects"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
)

# Download sound effects
ExternalProject_Add(opensound
    URL ${OPENSFX_URL}
    URL_HASH SHA256=${OPENSFX_SHA256}
    SOURCE_DIR "${TEMP_DIR}/opensound"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
)

# Download music
ExternalProject_Add(openmusic
    URL ${OPENMSX_URL}
    URL_HASH SHA256=${OPENMSX_SHA256}
    SOURCE_DIR "${TEMP_DIR}/openmusic"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
)

# Build graphics files using gxc
add_custom_target(build-graphics ALL
    DEPENDS gxc-tools
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TEMP_DIR}/graphics"
    COMMAND "${GXC_TOOLS_DIR}/gxc" build "${TEMP_DIR}/graphics/g2.dat" "${ORCT2_ROOT}/resources/g2/sprites.json"
    COMMAND "${GXC_TOOLS_DIR}/gxc" build "${TEMP_DIR}/graphics/fonts.dat" "${ORCT2_ROOT}/resources/fonts/sprites.json"
    COMMAND "${GXC_TOOLS_DIR}/gxc" build "${TEMP_DIR}/graphics/tracks.dat" "${ORCT2_ROOT}/resources/tracks/sprites.json"
    BYPRODUCTS
        "${TEMP_DIR}/graphics/g2.dat"
        "${TEMP_DIR}/graphics/fonts.dat"
        "${TEMP_DIR}/graphics/tracks.dat"
    COMMENT "Building graphics files with gxc"
)

# Copy all assets to Android assets directory
add_custom_target(prepare-assets ALL
    DEPENDS title-sequences objects opensound openmusic build-graphics
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DIR}"

    # Copy static data
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ORCT2_ROOT}/data/language" "${ASSETS_DIR}/language"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ORCT2_ROOT}/data/scenario_patches" "${ASSETS_DIR}/scenario_patches"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ORCT2_ROOT}/data/shaders" "${ASSETS_DIR}/shaders"

    # Copy generated graphics
    COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_DIR}/graphics/g2.dat" "${ASSETS_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_DIR}/graphics/fonts.dat" "${ASSETS_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${TEMP_DIR}/graphics/tracks.dat" "${ASSETS_DIR}/"

    # Copy title sequences
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}/title-sequences" "${ASSETS_DIR}/sequence"

    # Copy objects
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}/objects" "${ASSETS_DIR}/object"

    # Copy opensound assets (new .parkap format)
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}/opensound/assetpack" "${ASSETS_DIR}/assetpack"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}/opensound/object/official/audio" "${ASSETS_DIR}/object/official/audio"

    # Copy openmusic assets (new .parkap format)
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}/openmusic/assetpack" "${ASSETS_DIR}/assetpack"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TEMP_DIR}/openmusic/object/official/music" "${ASSETS_DIR}/object/official/music"

    COMMENT "Preparing all assets for Android APK"
)

add_library(openrct2 SHARED ${LIBOPENRCT2_SOURCES})
target_link_libraries(openrct2 android stdc++ log dl SDL2 png z zstd icu icuuc icudata crypto ssl freetype)

add_library(openrct2-ui SHARED ${OPENRCT2_GUI_SOURCES})
target_link_libraries(openrct2-ui openrct2 android stdc++ GLESv1_CM GLESv2 SDL2main speexdsp brotlicommon brotlidec bz2 freetype ogg vorbis vorbisfile FLAC)

# Make sure assets are prepared before building openrct2-ui
add_dependencies(openrct2-ui prepare-assets)

target_include_directories(openrct2 PRIVATE "${ORCT2_ROOT}/src/thirdparty/duktape/")
target_include_directories(openrct2 SYSTEM PRIVATE "${ORCT2_ROOT}/src/thirdparty")
target_include_directories(openrct2-ui PRIVATE "${ORCT2_ROOT}/src/thirdparty/duktape/")
target_include_directories(openrct2-ui PRIVATE "${ORCT2_ROOT}/src")
target_include_directories(openrct2-ui SYSTEM PRIVATE "${ORCT2_ROOT}/src/thirdparty")

target_include_directories(openrct2 PRIVATE "/opt/openrct2/include/nlohmann/../")
target_include_directories(openrct2-ui PRIVATE "/opt/openrct2/include/nlohmann/../")
