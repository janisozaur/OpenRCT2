#include "android_asset_manager.h"

#ifdef __ANDROID__
#include <cstring>
#include <chrono>

#define LOG_TAG "AndroidAssetManager"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGPERF(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "[PERF] " __VA_ARGS__)

// Critical assets that must be present for OpenRCT2 to function
const std::vector<std::string> AndroidAssetManager::CRITICAL_ASSETS = {
    "openrct2/g2.dat",
    "openrct2/fonts.dat",
    "openrct2/tracks.dat",
    "openrct2/language/en-GB.txt",
    "openrct2/assetpack"  // Directory check
};

AndroidAssetManager& AndroidAssetManager::getInstance() {
    static AndroidAssetManager instance;
    return instance;
}

bool AndroidAssetManager::initialize(AAssetManager* assetManager) {
    auto start = std::chrono::high_resolution_clock::now();
    LOGPERF("Asset manager initialization started");

    if (!assetManager) {
        LOGE("AssetManager is null");
        return false;
    }

    m_assetManager = assetManager;
    m_initialized = true;

    LOGI("Phase 2: Android AssetManager initialized successfully");

    // Validate critical assets are present
    auto validation_start = std::chrono::high_resolution_clock::now();
    if (!validateCriticalAssets()) {
        LOGE("Critical asset validation failed");
        return false;
    }

    auto validation_end = std::chrono::high_resolution_clock::now();
    auto validation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(validation_end - validation_start);
    LOGPERF("Critical asset validation took %lld ms", (long long)validation_duration.count());

    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOGPERF("Total asset manager initialization took %lld ms", (long long)total_duration.count());

    LOGI("✓ All critical assets validated");
    return true;
}

bool AndroidAssetManager::assetExists(const std::string& path) {
    if (!m_initialized) {
        LOGE("AssetManager not initialized");
        return false;
    }

    AAsset* asset = AAssetManager_open(m_assetManager, path.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) {
        AAsset_close(asset);
        return true;
    }
    return false;
}

std::vector<uint8_t> AndroidAssetManager::readAsset(const std::string& path) {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> data;

    if (!m_initialized) {
        LOGE("AssetManager not initialized");
        return data;
    }

    AAsset* asset = AAssetManager_open(m_assetManager, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset: %s", path.c_str());
        return data;
    }

    size_t size = AAsset_getLength(asset);
    data.resize(size);

    int bytesRead = AAsset_read(asset, data.data(), size);
    AAsset_close(asset);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (bytesRead != static_cast<int>(size)) {
        LOGE("Failed to read complete asset: %s (%d/%zu bytes)", path.c_str(), bytesRead, size);
        data.clear();
    } else {
        if (size > 1024 * 1024) { // Only log for files > 1MB
            LOGPERF("Read large asset: %s (%zu bytes) in %lld ms", path.c_str(), size, (long long)duration.count());
        }
        LOGD("Successfully read asset: %s (%zu bytes)", path.c_str(), size);
    }

    return data;
}

size_t AndroidAssetManager::getAssetSize(const std::string& path) {
    if (!m_initialized) {
        return 0;
    }

    AAsset* asset = AAssetManager_open(m_assetManager, path.c_str(), AASSET_MODE_UNKNOWN);
    if (!asset) {
        return 0;
    }

    size_t size = AAsset_getLength(asset);
    AAsset_close(asset);
    return size;
}

AAsset* AndroidAssetManager::openAsset(const std::string& path) {
    if (!m_initialized) {
        return nullptr;
    }

    return AAssetManager_open(m_assetManager, path.c_str(), AASSET_MODE_STREAMING);
}

std::vector<std::string> AndroidAssetManager::listAssets(const std::string& directory) {
    std::vector<std::string> assets;

    if (!m_initialized) {
        return assets;
    }

    AAssetDir* assetDir = AAssetManager_openDir(m_assetManager, directory.c_str());
    if (!assetDir) {
        LOGE("Failed to open asset directory: %s", directory.c_str());
        return assets;
    }

    const char* filename;
    while ((filename = AAssetDir_getNextFileName(assetDir)) != nullptr) {
        std::string fullPath = directory.empty() ? filename : directory + "/" + filename;
        assets.push_back(fullPath);
    }

    AAssetDir_close(assetDir);
    LOGD("Listed %zu assets in directory: %s", assets.size(), directory.c_str());
    return assets;
}

bool AndroidAssetManager::validateCriticalAssets() {
    if (!m_initialized) {
        return false;
    }

    LOGI("Validating critical assets...");

    for (const auto& assetPath : CRITICAL_ASSETS) {
        if (assetPath.back() == '/') {
            // Directory check
            auto files = listAssets(assetPath);
            if (files.empty()) {
                LOGE("Critical directory is empty: %s", assetPath.c_str());
                return false;
            }
        } else {
            // File check
            if (!assetExists(assetPath)) {
                LOGE("Critical asset missing: %s", assetPath.c_str());
                return false;
            }
        }
    }

    return true;
}

std::vector<AndroidAssetManager::AssetInfo> AndroidAssetManager::getAssetSummary() {
    std::vector<AssetInfo> summary;

    if (!m_initialized) {
        return summary;
    }

    // Get info for critical assets
    for (const auto& path : CRITICAL_ASSETS) {
        AssetInfo info;
        info.path = path;
        info.exists = assetExists(path);
        info.size = info.exists ? getAssetSize(path) : 0;
        summary.push_back(info);
    }

    return summary;
}

// C-style interface implementation
extern "C" {
    void android_asset_manager_init(AAssetManager* assetManager) {
        AndroidAssetManager::getInstance().initialize(assetManager);
    }

    int android_asset_exists(const char* path) {
        return AndroidAssetManager::getInstance().assetExists(path) ? 1 : 0;
    }

    uint8_t* android_asset_read(const char* path, size_t* size) {
        auto data = AndroidAssetManager::getInstance().readAsset(path);
        if (data.empty()) {
            *size = 0;
            return nullptr;
        }

        *size = data.size();
        uint8_t* result = new uint8_t[data.size()];
        std::memcpy(result, data.data(), data.size());
        return result;
    }

    void android_asset_free(uint8_t* data) {
        delete[] data;
    }

    int android_assets_validate() {
        return AndroidAssetManager::getInstance().validateCriticalAssets() ? 1 : 0;
    }
}

#else
// Non-Android stub implementations
AndroidAssetManager& AndroidAssetManager::getInstance() {
    static AndroidAssetManager instance;
    return instance;
}

extern "C" {
    void android_asset_manager_init(void* assetManager) {
        // Stub for non-Android platforms
    }

    int android_asset_exists(const char* path) {
        return 0;
    }

    uint8_t* android_asset_read(const char* path, size_t* size) {
        *size = 0;
        return nullptr;
    }

    void android_asset_free(uint8_t* data) {
        // Stub
    }

    int android_assets_validate() {
        return 0;
    }
}
#endif
