/**
 * Phase 2: Android Asset Manager JNI Bridge
 *
 * This file provides the JNI interface between Android Java/Kotlin code
 * and the native C++ Android Asset Manager.
 */

#include <jni.h>
#include "android_asset_manager.h"
#include "startup_profiler.h"

#ifdef __ANDROID__
#include <android/asset_manager_jni.h>

extern "C" {

/**
 * Initialize the native asset manager from Java AssetManager
 * Called from OpenRCT2Activity.onCreate() or similar
 */
JNIEXPORT jboolean JNICALL
Java_website_openrct2_OpenRCT2_initializeAssetManager(JNIEnv *env, jobject obj, jobject assetManager) {
    PROFILE_START("AssetManager_JNI_Init");

    AAssetManager* nativeAssetManager = AAssetManager_fromJava(env, assetManager);
    if (!nativeAssetManager) {
        PROFILE_END("AssetManager_JNI_Init");
        return JNI_FALSE;
    }

    android_asset_manager_init(nativeAssetManager);
    jboolean result = android_assets_validate() ? JNI_TRUE : JNI_FALSE;

    PROFILE_END("AssetManager_JNI_Init");
    return result;
}

/**
 * Validate that all critical assets are accessible
 * Can be called from Java to verify APK integrity
 */
JNIEXPORT jboolean JNICALL
Java_website_openrct2_OpenRCT2_validateAssets(JNIEnv *env, jobject obj) {
    return android_assets_validate() ? JNI_TRUE : JNI_FALSE;
}

/**
 * Check if a specific asset exists
 * Useful for conditional asset loading
 */
JNIEXPORT jboolean JNICALL
Java_website_openrct2_OpenRCT2_assetExists(JNIEnv *env, jobject obj, jstring path) {
    const char* nativePath = env->GetStringUTFChars(path, nullptr);
    jboolean result = android_asset_exists(nativePath) ? JNI_TRUE : JNI_FALSE;
    env->ReleaseStringUTFChars(path, nativePath);
    return result;
}

/**
 * Get the size of an asset without reading it
 * Useful for progress reporting during asset loading
 */
JNIEXPORT jlong JNICALL
Java_website_openrct2_OpenRCT2_getAssetSize(JNIEnv *env, jobject obj, jstring path) {
    const char* nativePath = env->GetStringUTFChars(path, nullptr);
    size_t size = 0;
    if (android_asset_exists(nativePath)) {
        // For simplicity, we'll return the size via the asset manager
        AndroidAssetManager& manager = AndroidAssetManager::getInstance();
        size = manager.getAssetSize(nativePath);
    }
    env->ReleaseStringUTFChars(path, nativePath);
    return static_cast<jlong>(size);
}

} // extern "C"

#else
// Non-Android stub implementations
extern "C" {

JNIEXPORT jboolean JNICALL
Java_website_openrct2_OpenRCT2_initializeAssetManager(JNIEnv *env, jobject obj, jobject assetManager) {
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_website_openrct2_OpenRCT2_validateAssets(JNIEnv *env, jobject obj) {
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_website_openrct2_OpenRCT2_assetExists(JNIEnv *env, jobject obj, jstring path) {
    return JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_website_openrct2_OpenRCT2_getAssetSize(JNIEnv *env, jobject obj, jstring path) {
    return 0;
}

} // extern "C"

#endif
