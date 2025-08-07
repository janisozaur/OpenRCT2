#pragma once

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#endif

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

/**
 * Phase 2: Advanced Asset Management - Android AAsset Integration
 *
 * This class provides direct access to assets embedded in the APK,
 * eliminating the need for file copying during build time.
 */
class AndroidAssetManager {
public:
    static AndroidAssetManager& getInstance();

#ifdef __ANDROID__
    // Initialize with JNI AssetManager
    bool initialize(AAssetManager* assetManager);

    // Check if asset exists in APK
    bool assetExists(const std::string& path);

    // Read entire asset into memory
    std::vector<uint8_t> readAsset(const std::string& path);

    // Get asset size without reading
    size_t getAssetSize(const std::string& path);

    // Open asset for streaming (for large files)
    AAsset* openAsset(const std::string& path);

    // List assets in directory
    std::vector<std::string> listAssets(const std::string& directory = "");

    // Validate critical assets are present
    bool validateCriticalAssets();

    // Get asset info for debugging
    struct AssetInfo {
        std::string path;
        size_t size;
        bool exists;
    };
    std::vector<AssetInfo> getAssetSummary();

private:
    AndroidAssetManager() = default;
    AAssetManager* m_assetManager = nullptr;
    bool m_initialized = false;

    // Critical assets that must be present
    static const std::vector<std::string> CRITICAL_ASSETS;
#else
    // Stub implementations for non-Android platforms
    bool initialize(void* assetManager) { return false; }
    bool assetExists(const std::string& path) { return false; }
    std::vector<uint8_t> readAsset(const std::string& path) { return {}; }
    size_t getAssetSize(const std::string& path) { return 0; }
    void* openAsset(const std::string& path) { return nullptr; }
    std::vector<std::string> listAssets(const std::string& directory = "") { return {}; }
    bool validateCriticalAssets() { return false; }

private:
    AndroidAssetManager() = default;
    bool m_initialized = false;
#endif
};

// C-style interface for OpenRCT2 integration
extern "C" {
#ifdef __ANDROID__
    // Initialize asset manager from JNI
    void android_asset_manager_init(AAssetManager* assetManager);
#else
    void android_asset_manager_init(void* assetManager);
#endif

    // Check if asset exists
    int android_asset_exists(const char* path);

    // Read asset data
    uint8_t* android_asset_read(const char* path, size_t* size);

    // Free asset data
    void android_asset_free(uint8_t* data);

    // Validate all required assets
    int android_assets_validate();
}
