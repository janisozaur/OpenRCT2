/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ZipAssetProvider.h"

#include "../Diagnostic.h"
#include "../core/IStream.hpp"
#include "../core/String.hpp"
#include "Platform.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <zip.h>

namespace OpenRCT2
{
    class ZipAssetProvider final : public IAssetProvider
    {
    private:
        zip_t* _zip;
        std::vector<AssetInfo> _assetList;
        std::mutex _mutex;

    public:
        ZipAssetProvider(const std::string& zipPath)
        {
            int32_t error;
            _zip = zip_open(zipPath.c_str(), ZIP_RDONLY, &error);
            if (_zip == nullptr)
            {
                throw std::runtime_error("Unable to open zip file: " + zipPath);
            }

            // Build asset list
            auto numEntries = zip_get_num_entries(_zip, 0);
            for (zip_int64_t i = 0; i < numEntries; i++)
            {
                zip_stat_t st;
                if (zip_stat_index(_zip, i, 0, &st) == 0)
                {
                    AssetInfo info;
                    info.Path = st.name;
                    info.Size = st.size;
                    _assetList.push_back(std::move(info));
                }
            }

            std::sort(_assetList.begin(), _assetList.end(), [](const AssetInfo& a, const AssetInfo& b) {
                return a.Path < b.Path;
            });
        }

        ~ZipAssetProvider() override
        {
            zip_close(_zip);
        }

        OpenRCT2::Platform::AssetCheckResult CheckAssetExists(u8string_view path) override
        {
            auto assetPath = std::string(path);
            auto it = std::lower_bound(
                _assetList.begin(), _assetList.end(), assetPath,
                [](const AssetInfo& a, const std::string& b) { return a.Path < b; });

            if (it != _assetList.end() && it->Path == assetPath)
            {
                return OpenRCT2::Platform::AssetCheckResult::Found;
            }
            return OpenRCT2::Platform::AssetCheckResult::NotFound;
        }

        OpenRCT2::Platform::AssetCheckResult CheckAssetDirectoryExists(u8string_view path) override
        {
            std::string prefix = std::string(path);
            if (!prefix.empty() && prefix.back() != '/')
            {
                prefix += '/';
            }

            auto it = std::lower_bound(
                _assetList.begin(), _assetList.end(), prefix,
                [](const AssetInfo& a, const std::string& b) { return a.Path < b; });

            if (it != _assetList.end() && String::startsWith(it->Path, prefix))
            {
                return OpenRCT2::Platform::AssetCheckResult::Found;
            }
            return OpenRCT2::Platform::AssetCheckResult::NotFound;
        }

        OpenRCT2::Platform::AssetFileOpenResult OpenAssetFile(u8string_view path) override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            std::string pathStr(path);
            auto assetFile = zip_fopen(_zip, pathStr.c_str(), 0);
            if (assetFile == nullptr)
            {
                return { OpenRCT2::Platform::AssetCheckResult::NotFound, nullptr, 0 };
            }

            zip_stat_t st;
            zip_stat(_zip, pathStr.c_str(), 0, &st);

            return { OpenRCT2::Platform::AssetCheckResult::Found, assetFile, st.size };
        }

        void CloseAssetFile(OpenRCT2::Platform::AssetHandle handle) override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            zip_fclose(static_cast<zip_file_t*>(handle));
        }

        uint64_t GetAssetPosition(OpenRCT2::Platform::AssetHandle handle) override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto pos = zip_ftell(static_cast<zip_file_t*>(handle));
            return pos >= 0 ? static_cast<uint64_t>(pos) : 0;
        }

        void SeekAsset(OpenRCT2::Platform::AssetHandle handle, int64_t offset, int32_t origin) override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            int whence;
            switch (origin)
            {
                case STREAM_SEEK_BEGIN:
                    whence = SEEK_SET;
                    break;
                case STREAM_SEEK_CURRENT:
                    whence = SEEK_CUR;
                    break;
                case STREAM_SEEK_END:
                    whence = SEEK_END;
                    break;
                default:
                    return;
            }
            zip_fseek(static_cast<zip_file_t*>(handle), static_cast<zip_int64_t>(offset), whence);
        }

        uint64_t ReadAsset(OpenRCT2::Platform::AssetHandle handle, void* buffer, uint64_t length) override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto readBytes = zip_fread(static_cast<zip_file_t*>(handle), buffer, length);
            return readBytes > 0 ? static_cast<uint64_t>(readBytes) : 0;
        }

        uint64_t TryReadAsset(OpenRCT2::Platform::AssetHandle handle, void* buffer, uint64_t length) override
        {
            return ReadAsset(handle, buffer, length);
        }

        const std::vector<AssetInfo>& GetAssetList() override
        {
            return _assetList;
        }
    };

    std::unique_ptr<IAssetProvider> CreateZipAssetProvider(const std::string& zipPath)
    {
        try
        {
            return std::make_unique<ZipAssetProvider>(zipPath);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("%s", e.what());
            return nullptr;
        }
    }
} // namespace OpenRCT2
