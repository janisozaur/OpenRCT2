set(TITLE_SEQUENCE_VERSION "0.4.14")
set(TITLE_SEQUENCE_URL     "https://github.com/OpenRCT2/title-sequences/releases/download/v${TITLE_SEQUENCE_VERSION}/title-sequences.zip")
set(TITLE_SEQUENCE_SHA256  "140df714e806fed411cc49763e7f16b0fcf2a487a57001d1e50fce8f9148a9f3")

set(OBJECTS_VERSION "1.7.3")
set(OBJECTS_URL     "https://github.com/OpenRCT2/objects/releases/download/v${OBJECTS_VERSION}/objects.zip")
set(OBJECTS_SHA256  "06b90f3e19c216752df441d551b26a9e3e1ba7755bdd2102504b73bf993608be")

set(OPENSFX_VERSION "1.0.6")
set(OPENSFX_URL     "https://github.com/OpenRCT2/OpenSoundEffects/releases/download/v${OPENSFX_VERSION}/opensound.zip")
set(OPENSFX_SHA256  "06b90f3e19c216752df441d551b26a9e3e1ba7755bdd2102504b73bf993608be")

set(OPENMSX_VERSION "1.6.1")
set(OPENMSX_URL     "https://github.com/OpenRCT2/OpenMusic/releases/download/v${OPENMSX_VERSION}/openmusic.zip")
set(OPENMSX_SHA256  "994b350d3b180ee1cb9619fe27f7ebae3a1a5232840c4bd47a89f33fa89de1a1")

set(REPLAYS_VERSION "0.0.89")
set(REPLAYS_URL     "https://github.com/OpenRCT2/replays/releases/download/v${REPLAYS_VERSION}/replays.zip")
set(REPLAYS_SHA256  "04607bb1f67a0f31d841ed70b38d65b8f7a9e19749e414ff74b8a434bc90b42a")

set(GXC_TOOLS_URL "https://github.com/IntelOrca/libsawyer/releases/download/v1.3.0/libsawyer-tools-linux-x64.tar.gz")

function(add_asset_download_targets)
    # Parse optional arguments
    set(options ANDROID_BUILD)
    set(oneValueArgs TEMP_DIR)
    cmake_parse_arguments(ASSET_DL "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT ASSET_DL_TEMP_DIR)
        set(ASSET_DL_TEMP_DIR "${CMAKE_BINARY_DIR}/temp")
    endif()

    include(ExternalProject)

    # Download title sequences
    ExternalProject_Add(title-sequences
        URL ${TITLE_SEQUENCE_URL}
        URL_HASH SHA256=${TITLE_SEQUENCE_SHA256}
        SOURCE_DIR "${ASSET_DL_TEMP_DIR}/title-sequences"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD 1
    )

    # Download objects
    ExternalProject_Add(objects
        URL ${OBJECTS_URL}
        URL_HASH SHA256=${OBJECTS_SHA256}
        SOURCE_DIR "${ASSET_DL_TEMP_DIR}/objects"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD 1
    )

    # Download sound effects
    ExternalProject_Add(opensound
        URL ${OPENSFX_URL}
        URL_HASH SHA256=${OPENSFX_SHA256}
        SOURCE_DIR "${ASSET_DL_TEMP_DIR}/opensound"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD 1
    )

    # Download music
    ExternalProject_Add(openmusic
        URL ${OPENMSX_URL}
        URL_HASH SHA256=${OPENMSX_SHA256}
        SOURCE_DIR "${ASSET_DL_TEMP_DIR}/openmusic"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD 1
    )

    # Download replays (desktop builds only, not needed for Android)
    if(NOT ASSET_DL_ANDROID_BUILD)
        ExternalProject_Add(replays
            URL ${REPLAYS_URL}
            URL_HASH SHA256=${REPLAYS_SHA256}
            SOURCE_DIR "${ASSET_DL_TEMP_DIR}/replays"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD 1
        )
    endif()

    # Download GXC tools (Android builds only)
    if(ASSET_DL_ANDROID_BUILD)
        set(GXC_TOOLS_DIR "${CMAKE_BINARY_DIR}/gxc-tools")
        ExternalProject_Add(gxc-tools
            URL ${GXC_TOOLS_URL}
            SOURCE_DIR "${GXC_TOOLS_DIR}"
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD 1
        )
    endif()
endfunction()
