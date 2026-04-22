include("${CMAKE_CURRENT_LIST_DIR}/download.cmake")
download_openrct2_zip(
    DOWNLOAD_DIR "${DOWNLOAD_DIR}"
    ZIP_URL "${ZIP_URL}"
    SHA256 "${SHA256}"
    SKIP_IF_EXISTS "${SKIP_IF_EXISTS}"
)
