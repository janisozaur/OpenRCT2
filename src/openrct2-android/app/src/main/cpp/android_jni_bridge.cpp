/**
 * Phase 2: Android Asset Manager JNI Bridge
 *
 * This file provides the JNI interface between Android Java/Kotlin code
 * and the native C++ Android Asset Manager.
 */

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include "android_asset_manager.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "OpenRCT2", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OpenRCT2", __VA_ARGS__)

extern "C" {

JNIEXPORT void JNICALL
Java_io_openrct2_GameActivity_nativeSetupAssetManager(JNIEnv *env, jobject thiz, jobject assetManager) {
    LOGI("Setting up native asset manager");

    // Convert Java AssetManager to native AAssetManager
    AAssetManager* aAssetManager = AAssetManager_fromJava(env, assetManager);
    if (aAssetManager) {
        // Initialize the AndroidAssetManager singleton
        AndroidAssetManager::getInstance().initialize(aAssetManager);
        LOGI("AndroidAssetManager initialized successfully");
    } else {
        LOGE("Failed to convert Java AssetManager to native AAssetManager");
    }
}

} // extern "C"
