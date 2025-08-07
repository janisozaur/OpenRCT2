/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef __ANDROID__

    #include "Platform.h"

    #include "../Diagnostic.h"
    #include "../core/File.h"
    #include "../core/Guard.hpp"
    #include "../localisation/Language.h"
    #include "../../openrct2-android/app/src/main/cpp/android_asset_manager.h"

    #include <SDL.h>
    #include <jni.h>
    #include <memory>

AndroidClassLoader::~AndroidClassLoader()
{
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    env->DeleteGlobalRef(_classLoader);
}

jobject AndroidClassLoader::_classLoader;
jmethodID AndroidClassLoader::_findClassMethod;

// Initialized in JNI_OnLoad. Cannot be initialized here as JVM is not
// available until after JNI_OnLoad is called.
static std::shared_ptr<AndroidClassLoader> acl;

namespace OpenRCT2::Platform
{
    std::string GetFolderPath(SpecialFolder folder)
    {
        // Android builds currently only read from /sdcard/openrct2*
        switch (folder)
        {
            case SpecialFolder::userCache:
            case SpecialFolder::userConfig:
            case SpecialFolder::userData:
            case SpecialFolder::userHome:
                return "/sdcard";
            default:
                return std::string();
        }
    }

    std::string GetDocsPath()
    {
        return std::string();
    }

    std::string GetInstallPath()
    {
        return "/sdcard/openrct2";
    }

    std::string GetCurrentExecutablePath()
    {
        // On Android, we don't have a traditional executable path like on desktop platforms.
        // Instead, we return the APK path which contains our embedded assets.
        // This is used by the config system to search for game data files.

        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        if (!env) {
            LOG_ERROR("JNI environment not available in GetCurrentExecutablePath");
            return "/data/app/io.openrct2/base.apk";  // fallback path
        }

        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!activity) {
            LOG_ERROR("Android activity not available in GetCurrentExecutablePath");
            return "/data/app/io.openrct2/base.apk";  // fallback path
        }

        jclass activityClass = env->GetObjectClass(activity);
        jmethodID getPackageCodePath = env->GetMethodID(activityClass, "getPackageCodePath", "()Ljava/lang/String;");

        if (!getPackageCodePath) {
            LOG_ERROR("Failed to get getPackageCodePath method");
            env->DeleteLocalRef(activity);
            env->DeleteLocalRef(activityClass);
            return "/data/app/io.openrct2/base.apk";  // fallback path
        }

        jstring jniString = static_cast<jstring>(env->CallObjectMethod(activity, getPackageCodePath));
        if (!jniString) {
            LOG_ERROR("Failed to get package code path");
            env->DeleteLocalRef(activity);
            env->DeleteLocalRef(activityClass);
            return "/data/app/io.openrct2/base.apk";  // fallback path
        }

        const char* jniChars = env->GetStringUTFChars(jniString, nullptr);
        std::string apkPath = jniChars;

        env->ReleaseStringUTFChars(jniString, jniChars);
        env->DeleteLocalRef(jniString);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(activityClass);

        LOG_INFO("Current executable path (APK): %s", apkPath.c_str());
        return apkPath;
    }

    u8string StrDecompToPrecomp(u8string_view input)
    {
        return u8string(input);
    }

    bool HandleSpecialCommandLineArgument(const char* argument)
    {
        return false;
    }

    uint16_t GetLocaleLanguage()
    {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID getDefaultLocale = env->GetMethodID(
            activityClass, "getDefaultLocale", "([Ljava/lang/String;)Ljava/lang/String;");

        jobjectArray jLanguageTags = env->NewObjectArray(
            LANGUAGE_COUNT, env->FindClass("java/lang/String"), env->NewStringUTF(""));

        for (int32_t i = 1; i < LANGUAGE_COUNT; ++i)
        {
            jstring jTag = env->NewStringUTF(LanguagesDescriptors[i].locale);
            env->SetObjectArrayElement(jLanguageTags, i, jTag);
        }

        jstring jniString = static_cast<jstring>(env->CallObjectMethod(activity, getDefaultLocale, jLanguageTags));

        const char* jniChars = env->GetStringUTFChars(jniString, nullptr);
        std::string defaultLocale = jniChars;

        env->ReleaseStringUTFChars(jniString, jniChars);
        for (int32_t i = 0; i < LANGUAGE_COUNT; ++i)
        {
            jobject strToFree = env->GetObjectArrayElement(jLanguageTags, i);
            env->DeleteLocalRef(strToFree);
        }
        env->DeleteLocalRef(jLanguageTags);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(activityClass);

        return LanguageGetIDFromLocale(defaultLocale.c_str());
    }

    CurrencyType GetLocaleCurrency()
    {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID getDefaultLocale = env->GetMethodID(activityClass, "getLocaleCurrency", "()Ljava/lang/String;");

        jstring jniString = static_cast<jstring>(env->CallObjectMethod(activity, getDefaultLocale));

        const char* jniChars = env->GetStringUTFChars(jniString, nullptr);
        std::string localeCurrencyCode = jniChars;

        env->ReleaseStringUTFChars(jniString, jniChars);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(activityClass);

        return Platform::GetCurrencyValue(localeCurrencyCode.c_str());
    }

    MeasurementFormat GetLocaleMeasurementFormat()
    {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID getIsImperialLocaleMeasurementFormat = env->GetMethodID(
            activityClass, "isImperialLocaleMeasurementFormat", "()Z");

        jboolean isImperial = env->CallBooleanMethod(activity, getIsImperialLocaleMeasurementFormat);

        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(activityClass);
        return isImperial == JNI_TRUE ? MeasurementFormat::Imperial : MeasurementFormat::Metric;
    }

    std::string GetSteamPath()
    {
        return {};
    }

    u8string GetRCT1SteamDir()
    {
        return {};
    }

    u8string GetRCT2SteamDir()
    {
        return {};
    }

    u8string GetRCTClassicSteamDir()
    {
        return {};
    }

    #ifndef DISABLE_TTF
    std::string GetFontPath(const TTFFontDescriptor& font)
    {
        auto expectedPath = std::string("/system/fonts/") + std::string(font.filename);
        if (File::Exists(expectedPath))
        {
            return expectedPath;
        }

        return {};
    }
    #endif

    float GetDefaultScale()
    {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());

        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID getDefaultScale = env->GetMethodID(activityClass, "getDefaultScale", "()F");

        jfloat displayScale = env->CallFloatMethod(activity, getDefaultScale);

        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(activityClass);

        return displayScale;
    }

    jclass AndroidFindClass(JNIEnv* env, std::string_view name)
    {
        return static_cast<jclass>(env->CallObjectMethod(
            AndroidClassLoader::_classLoader, AndroidClassLoader::_findClassMethod,
            env->NewStringUTF(std::string(name).c_str())));
    }

    std::vector<std::string_view> GetSearchablePathsRCT1()
    {
        return { "/sdcard/rct1" };
    }

    std::vector<std::string_view> GetSearchablePathsRCT2()
    {
        return { "/sdcard/rct2" };
    }

    bool TryLoadFile(const std::string& path, std::vector<uint8_t>& data)
    {
        // Initialize AndroidClassLoader if needed
        static bool classLoaderInitialized = false;
        if (!classLoaderInitialized) {
            try {
                InitializeAndroidClassLoader();
                classLoaderInitialized = true;
                LOG_INFO("AndroidClassLoader initialized for file loading");
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to initialize AndroidClassLoader: %s", e.what());
                // Continue with fallback loading
            }
        }

        // Phase 2: Try loading from embedded assets first on Android
        std::string pathStr = path;

        // Convert filesystem path to asset path
        // Remove leading "/sdcard/openrct2/" and replace with "openrct2/"
        size_t sdcardPos = pathStr.find("/sdcard/openrct2/");
        if (sdcardPos != std::string::npos) {
            std::string assetPath = "openrct2/" + pathStr.substr(sdcardPos + 17); // 17 = length of "/sdcard/openrct2/"
            LOG_VERBOSE("Trying to load file from embedded asset: %s", assetPath.c_str());

            auto& assetManager = AndroidAssetManager::getInstance();
            if (assetManager.assetExists(assetPath)) {
                auto assetData = assetManager.readAsset(assetPath);
                if (!assetData.empty()) {
                    data = assetData;
                    LOG_INFO("Successfully loaded file from embedded asset: %s (%zu bytes)", assetPath.c_str(), data.size());
                    return true;
                } else {
                    LOG_ERROR("Failed to read data from embedded asset: %s", assetPath.c_str());
                }
            } else {
                LOG_VERBOSE("Asset not found in embedded assets: %s", assetPath.c_str());
            }
        }

        // Fall back to filesystem if asset loading failed
        LOG_VERBOSE("Falling back to filesystem for file: %s", path.c_str());
        return false; // Let the caller handle filesystem loading
    }
} // namespace OpenRCT2::Platform

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* pjvm, void* reserved)
{
    LOG_INFO("JNI_OnLoad called");

    // Store the JavaVM for later use
    static JavaVM* g_jvm = pjvm;

    // Don't initialize AndroidClassLoader here - it will be initialized when needed
    // This prevents crashes when SDL JNI environment isn't available yet

    return JNI_VERSION_1_6;
}

void InitializeAndroidClassLoader()
{
    if (!acl)
    {
        try {
            acl = std::make_shared<AndroidClassLoader>();
            LOG_INFO("AndroidClassLoader initialized successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize AndroidClassLoader: %s", e.what());
        }
    }
}

AndroidClassLoader::AndroidClassLoader()
{
    LOG_INFO("Obtaining JNI class loader");

    // This is a workaround to be able to call JNI's ClassLoader from non-main
    // thread, based on https://stackoverflow.com/a/16302771

    // Wait for SDL to be ready
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOG_ERROR("JNI environment not available for AndroidClassLoader");
        throw std::runtime_error("JNI environment not available");
    }

    // Take an arbitrary class. While the class does not really matter, it
    // makes sense to use one that's most likely already loaded and is unlikely
    // to be removed from code.
    auto randomClass = env->FindClass("io/openrct2/MainActivity");
    if (!randomClass) {
        LOG_ERROR("Failed to find MainActivity class");
        throw std::runtime_error("Failed to find MainActivity class");
    }

    jclass classClass = env->GetObjectClass(randomClass);

    // Get its class loader
    auto classLoaderClass = env->FindClass("java/lang/ClassLoader");
    auto getClassLoaderMethod = env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");

    // Store the class loader and its findClass method for future use
    _classLoader = env->NewGlobalRef(env->CallObjectMethod(randomClass, getClassLoaderMethod));
    _findClassMethod = env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
}

#endif
