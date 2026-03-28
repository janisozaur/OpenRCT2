/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../Date.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <cpuid.h>
    #define OpenRCT2_CPUID_GNUC_X86
#elif defined(_MSC_VER) && (_MSC_VER >= 1500) && (defined(_M_X64) || defined(_M_IX86)) // VS2008
    #include <intrin.h>
    #include <nmmintrin.h>
    #define OpenRCT2_CPUID_MSVC_X86
#endif

#include "../Context.h"
#include "../core/CallingConventions.h"
#include "../core/File.h"
#include "../core/Path.hpp"
#include "../core/FileSystem.hpp"
#include "../core/String.hpp"
#include "../localisation/Currency.h"
#include "Platform.h"
#include "IAssetProvider.h"
#include "ZipAssetProvider.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <thread>

#ifdef _WIN32
static constexpr std::array _prohibitedCharacters = { '<', '>', '*', '\\', ':', '|', '?', '"', '/' };
#else
static constexpr std::array _prohibitedCharacters = { '/' };
#endif

namespace OpenRCT2::Platform
{
    CurrencyType GetCurrencyValue(const char* currCode)
    {
        if (currCode == nullptr || strlen(currCode) < 3)
        {
            return CurrencyType::pounds;
        }

        for (int32_t currency = 0; currency < EnumValue(CurrencyType::count); ++currency)
        {
            if (strncmp(currCode, CurrencyDescriptors[currency].isoCode, 3) == 0)
            {
                return static_cast<CurrencyType>(currency);
            }
        }

        return CurrencyType::pounds;
    }

    RealWorldDate GetDateLocal()
    {
        auto time = std::time(nullptr);
        auto localTime = std::localtime(&time);

        RealWorldDate outDate;
        outDate.day = localTime->tm_mday;
        outDate.day_of_week = localTime->tm_wday;
        outDate.month = localTime->tm_mon + 1;
        outDate.year = localTime->tm_year + 1900;
        return outDate;
    }

    RealWorldTime GetTimeLocal()
    {
        auto time = std::time(nullptr);
        auto localTime = std::localtime(&time);

        RealWorldTime outTime;
        outTime.hour = localTime->tm_hour;
        outTime.minute = localTime->tm_min;
        outTime.second = localTime->tm_sec;
        return outTime;
    }

#ifndef __ANDROID__
    static std::unique_ptr<IAssetProvider> _assetProvider;

    static void EnsureAssetProvider()
    {
        if (_assetProvider == nullptr)
        {
            auto exeDir = GetCurrentExecutableDirectory();
            auto zipPath = Path::Combine(exeDir, u8"data.zip");

            std::error_code ec;
            if (fs::exists(fs::u8path(zipPath), ec))
            {
                _assetProvider = CreateZipAssetProvider(zipPath);
            }
        }
    }

    static u8string_view GetBundledAssetPath(u8string_view path)
    {
        auto installPath = GetInstallPath();
        // If installPath is the ZIP file itself, it won't be a directory.
        // We only want to handle paths that are relative to the install path.
        if (String::startsWith(path, installPath))
        {
            auto relative = path.substr(installPath.length());
            if (!relative.empty() && relative[0] == '/')
            {
                return relative.substr(1);
            }
            return relative;
        }
        return {};
    }

    AssetCheckResult CheckAssetDirectoryExists(u8string_view path)
    {
        // Precedence: directory first. Since File::Exists already handles the directory check
        // before calling this, we just need to make sure we don't interfere if it's a real directory.
        // Wait, File::Exists calls Platform::CheckAssetExists FIRST.
        // So we MUST check if the physical path exists as a directory first.
        if (Path::DirectoryExists(path))
        {
            return AssetCheckResult::NotApplicable;
        }

        EnsureAssetProvider();
        if (_assetProvider != nullptr)
        {
            auto bundledPath = GetBundledAssetPath(path);
            if (!bundledPath.empty())
            {
                return _assetProvider->CheckAssetDirectoryExists(bundledPath);
            }
        }
        return AssetCheckResult::NotApplicable;
    }

    AssetCheckResult CheckAssetExists(u8string_view path)
    {
        // Precedence: directory first. Check physical file system.
        std::error_code ec;
        if (fs::exists(fs::u8path(path), ec))
        {
            return AssetCheckResult::NotApplicable;
        }

        EnsureAssetProvider();
        if (_assetProvider != nullptr)
        {
            auto bundledPath = GetBundledAssetPath(path);
            if (!bundledPath.empty())
            {
                return _assetProvider->CheckAssetExists(bundledPath);
            }
        }
        return AssetCheckResult::NotApplicable;
    }

    AssetFileOpenResult OpenAssetFile(u8string_view path)
    {
        // Precedence: directory first. Check physical file system.
        std::error_code ec;
        if (fs::exists(fs::u8path(path), ec))
        {
            return AssetFileOpenResult{ AssetCheckResult::NotApplicable, nullptr, 0 };
        }

        EnsureAssetProvider();
        if (_assetProvider != nullptr)
        {
            auto bundledPath = GetBundledAssetPath(path);
            if (!bundledPath.empty())
            {
                return _assetProvider->OpenAssetFile(bundledPath);
            }
        }
        return AssetFileOpenResult{ AssetCheckResult::NotApplicable, nullptr, 0 };
    }

    void CloseAssetFile(AssetHandle handle)
    {
        if (_assetProvider != nullptr)
        {
            _assetProvider->CloseAssetFile(handle);
        }
    }

    uint64_t GetAssetPosition(AssetHandle handle)
    {
        if (_assetProvider != nullptr)
        {
            return _assetProvider->GetAssetPosition(handle);
        }
        return 0;
    }

    void SeekAsset(AssetHandle handle, int64_t offset, int32_t origin)
    {
        if (_assetProvider != nullptr)
        {
            _assetProvider->SeekAsset(handle, offset, origin);
        }
    }

    uint64_t ReadAsset(AssetHandle handle, void* buffer, uint64_t length)
    {
        if (_assetProvider != nullptr)
        {
            return _assetProvider->ReadAsset(handle, buffer, length);
        }
        return 0;
    }

    uint64_t TryReadAsset(AssetHandle handle, void* buffer, uint64_t length)
    {
        if (_assetProvider != nullptr)
        {
            return _assetProvider->TryReadAsset(handle, buffer, length);
        }
        return 0;
    }

    u8string GetAssetPath()
    {
        return {};
    }

    const std::vector<AssetInfo>& GetAssetList()
    {
        static const std::vector<AssetInfo> empty;
        EnsureAssetProvider();
        if (_assetProvider != nullptr)
        {
            return _assetProvider->GetAssetList();
        }
        return empty;
    }
#endif

    std::optional<RCT2Variant> classifyGamePath(std::string_view path)
    {
        auto combinedPath = Path::ResolveCasing(Path::Combine(path, u8"Data", u8"g1.dat"));
        if (File::Exists(combinedPath))
            return std::make_optional<RCT2Variant>(RCT2Variant::rct2Original);

        combinedPath = Path::ResolveCasing(Path::Combine(path, kRCTClassicWindowsDataFolder, u8"g1.dat"));
        if (File::Exists(combinedPath))
            return std::make_optional<RCT2Variant>(RCT2Variant::rctClassicWindows);

        combinedPath = Path::ResolveCasing(Path::Combine(path, kRCTClassicMacOSDataFolder, u8"g1.dat"));
        if (File::Exists(combinedPath))
            return std::make_optional<RCT2Variant>(RCT2Variant::rctClassicMac);

        combinedPath = Path::ResolveCasing(Path::Combine(path, kRCTClassicPlusMacOSDataFolder, u8"g1.dat"));
        if (File::Exists(combinedPath))
            return std::make_optional<RCT2Variant>(RCT2Variant::rctClassicPlusMac);

        return std::nullopt;
    }

    bool OriginalGameDataExists(std::string_view path)
    {
        return classifyGamePath(path) != std::nullopt;
    }

    std::string SanitiseFilename(std::string_view originalName)
    {
        auto sanitised = std::string(originalName);
        std::replace_if(
            sanitised.begin(), sanitised.end(),
            [](const std::string::value_type& ch) -> bool {
                return std::find(_prohibitedCharacters.begin(), _prohibitedCharacters.end(), ch) != _prohibitedCharacters.end();
            },
            '_');
        sanitised = String::trim(sanitised);
        return sanitised;
    }

    bool IsFilenameValid(u8string_view fileName)
    {
        return fileName.find_first_of(_prohibitedCharacters.data(), 0, _prohibitedCharacters.size()) == fileName.npos;
    }

#ifndef __ANDROID__
    float GetDefaultScale()
    {
        return 1;
    }
#endif

    void Sleep(uint32_t ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    static const auto _processStartTime = std::chrono::high_resolution_clock::now();

    uint32_t GetTicks()
    {
        const auto processTime = std::chrono::high_resolution_clock::now() - _processStartTime;
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(processTime).count());
    }

#ifdef OPENRCT2_X86
    static bool CPUIDX86(uint32_t* cpuid_outdata, int32_t eax)
    {
    #if defined(OpenRCT2_CPUID_GNUC_X86)
        int ret = __get_cpuid(eax, &cpuid_outdata[0], &cpuid_outdata[1], &cpuid_outdata[2], &cpuid_outdata[3]);
        return ret == 1;
    #elif defined(OpenRCT2_CPUID_MSVC_X86)
        __cpuid(reinterpret_cast<int*>(cpuid_outdata), static_cast<int>(eax));
        return true;
    #else
        return false;
    #endif
    }
#endif // OPENRCT2_X86

    bool SSE41Available()
    {
#ifdef OPENRCT2_X86
        // SSE4.1 support is declared as the 19th bit of ECX with CPUID(EAX = 1).
        uint32_t regs[4] = { 0 };
        if (CPUIDX86(regs, 1))
        {
            return (regs[2] & (1 << 19));
        }
#endif
        return false;
    }

    bool AVX2Available()
    {
#ifdef OPENRCT2_X86
        // For GCC and similar use the builtin function, as cpuid changed its semantics in
        // https://github.com/gcc-mirror/gcc/commit/132fa33ce998df69a9f793d63785785f4b93e6f1
        // which causes it to ignore subleafs, but the new function is unavailable on
        // Ubuntu 18.04's toolchains.
    #if defined(OpenRCT2_CPUID_GNUC_X86) && (!defined(__FreeBSD__) || (__FreeBSD__ > 10))
        return __builtin_cpu_supports("avx2");
    #else
        // AVX2 support is declared as the 5th bit of EBX with CPUID(EAX = 7, ECX = 0).
        uint32_t regs[4] = { 0 };
        if (CPUIDX86(regs, 7))
        {
            bool avxCPUSupport = (regs[1] & (1 << 5)) != 0;
            if (avxCPUSupport)
            {
                // Need to check if OS also supports the register of xmm/ymm
                // This check has to be conditional, otherwise INVALID_INSTRUCTION exception.
                uint64_t xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
                avxCPUSupport = (xcrFeatureMask & 0x6) || false;
            }
            return avxCPUSupport;
        }
    #endif
#endif
        return false;
    }

    bool SteamPaths::isSteamPresent() const
    {
        return !roots.empty();
    }

    u8string SteamPaths::getDownloadDepotFolder(u8string_view steamroot, const SteamGameData& data) const
    {
        return Path::Combine(
            steamroot, downloadDepotFolder, "app_" + std::to_string(data.appId), "depot_" + std::to_string(data.depotId));
    }

    bool triggerSteamDownload()
    {
        const auto steamPaths = GetSteamPaths();
        if (!steamPaths.isSteamPresent() || steamPaths.manifests.empty())
            return false;

        const auto manifestsDir = Path::Combine(steamPaths.roots[0], steamPaths.manifests);
        const std::array<SteamGameData, 3> gamesToTrigger = { kSteamRCT2Data, kSteamRCTCData, kSteamRCT1Data };
        for (const auto& game : gamesToTrigger)
        {
            auto fullFilename = Path::Combine(manifestsDir, "appmanifest_" + std::to_string(game.appId) + ".acf");
            // If the file exists, we assume a download has been triggered already.
            if (File::Exists(fullFilename))
                continue;

            // clang-format off
            auto buffer = u8string("\"AppState\"\r\n") + u8string("{\r\n")
                + u8string("	\"AppID\"	\"" + std::to_string(game.appId) + "\"\r\n")
                + u8string("	\"Universe\"	\"1\"\r\n")
                + u8string("	\"installdir\"	\"" + game.nativeFolder + "\"\r\n")
                + u8string("	\"StateFlags\"	\"1026\"\r\n") + u8string("}\r\n");
            // clang-format on
            File::WriteAllBytes(fullFilename, buffer.data(), buffer.size());
        }

        return true;
    }

} // namespace OpenRCT2::Platform
