/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/stat.h>
#endif

#include "../Diagnostic.h"
#include "../platform/Platform.h"
#include "File.h"
#include "FileStream.h"
#include "String.hpp"

#ifdef __ANDROID__
    #include <android/asset_manager.h>
#endif

#include <fstream>

namespace OpenRCT2::File
{
    bool Exists(u8string_view path)
    {
#ifdef __ANDROID__
        if (String::startsWith(path, Platform::kAndroidAssetPathPrefix))
        {
            auto assetManager = static_cast<AAssetManager*>(Platform::GetAssetManager());
            if (assetManager != nullptr)
            {
                std::string assetPath = std::string(path.substr(Platform::kAndroidAssetPathPrefix.length()));
                auto asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_UNKNOWN);
                if (asset != nullptr)
                {
                    AAsset_close(asset);
                    return true;
                }

                // Check if it is a directory
                auto dir = AAssetManager_openDir(assetManager, assetPath.c_str());
                if (dir != nullptr)
                {
                    // AAssetManager_openDir returns a non-null pointer even if the directory does not exist,
                    // but calling AAssetDir_getNextFileName will return null.
                    // However, we can't easily tell if it's an empty directory or a non-existent one.
                    // But for assets, we usually know what we are looking for.
                    auto firstFile = AAssetDir_getNextFileName(dir);
                    AAssetDir_close(dir);
                    return firstFile != nullptr;
                }
            }
            return false;
        }
#endif
        fs::path file = fs::u8path(path);
        LOG_VERBOSE("Checking if file exists: %s", u8string(path).c_str());
        std::error_code ec;
        const auto result = fs::exists(file, ec);
        return result && ec.value() == 0;
    }

    bool Copy(u8string_view srcPath, u8string_view dstPath, bool overwrite)
    {
        if (!overwrite && Exists(dstPath))
        {
            LOG_WARNING("File::Copy(): Not overwriting %s, because overwrite flag == false", u8string(dstPath).c_str());
            return false;
        }

        std::error_code ec;
        const auto result = fs::copy_file(fs::u8path(srcPath), fs::u8path(dstPath), ec);
        return result && ec.value() == 0;
    }

    bool Delete(u8string_view path)
    {
        std::error_code ec;
        const auto result = fs::remove(fs::u8path(path), ec);
        return result && ec.value() == 0;
    }

    bool Move(u8string_view srcPath, u8string_view dstPath)
    {
        std::error_code ec;
        fs::rename(fs::u8path(srcPath), fs::u8path(dstPath), ec);
        return ec.value() == 0;
    }

    std::vector<uint8_t> ReadAllBytes(u8string_view path)
    {
        FileStream fs(path, FileMode::open);
        std::vector<uint8_t> result;
        auto fsize = fs.GetLength();
        if (fsize > SIZE_MAX)
        {
            u8string message = String::stdFormat(
                "'%s' exceeds maximum length of %lld bytes.", u8string(path).c_str(), SIZE_MAX);
            throw IOException(message);
        }
        else
        {
            result.resize(static_cast<size_t>(fsize));
            fs.Read(result.data(), result.size());
        }
        return result;
    }

    u8string ReadAllText(u8string_view path)
    {
        auto bytes = ReadAllBytes(path);
        // TODO skip BOM
        u8string result(bytes.size(), 0);
        std::copy(bytes.begin(), bytes.end(), result.begin());
        return result;
    }

    std::vector<u8string> ReadAllLines(u8string_view path)
    {
        std::vector<u8string> lines;
        auto data = ReadAllBytes(path);
        auto lineStart = reinterpret_cast<const char*>(data.data());
        auto ch = lineStart;
        char lastC = 0;
        for (size_t i = 0; i < data.size(); i++)
        {
            char c = *ch;
            if (c == '\n' && lastC == '\r')
            {
                // Ignore \r\n
                lineStart = ch + 1;
            }
            else if (c == '\n' || c == '\r')
            {
                lines.emplace_back(lineStart, ch - lineStart);
                lineStart = ch + 1;
            }
            lastC = c;
            ch++;
        }

        // Last line
        lines.emplace_back(lineStart, ch - lineStart);
        return lines;
    }

    void WriteAllBytes(u8string_view path, const void* buffer, size_t length)
    {
        auto fs = FileStream(path, FileMode::write);
        fs.Write(buffer, length);
    }

    uint64_t GetLastModified(u8string_view path)
    {
        return Platform::GetLastModified(path);
    }

    uint64_t GetSize(u8string_view path)
    {
        return Platform::GetFileSize(path);
    }
} // namespace OpenRCT2::File
