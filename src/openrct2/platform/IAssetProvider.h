/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "AssetTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace OpenRCT2
{
    /**
     * Provides access to bundled assets (e.g. Android assets, ZIP archives).
     */
    struct IAssetProvider
    {
        virtual ~IAssetProvider() = default;

        /**
         * Checks if an asset exists at the given path.
         */
        virtual OpenRCT2::Platform::AssetCheckResult CheckAssetExists(u8string_view path) = 0;

        /**
         * Checks if a directory exists at the given path.
         */
        virtual OpenRCT2::Platform::AssetCheckResult CheckAssetDirectoryExists(u8string_view path) = 0;

        /**
         * Opens an asset for reading.
         */
        virtual OpenRCT2::Platform::AssetFileOpenResult OpenAssetFile(u8string_view path) = 0;

        /**
         * Closes an asset opened with OpenAssetFile.
         */
        virtual void CloseAssetFile(OpenRCT2::Platform::AssetHandle handle) = 0;

        /**
         * Gets the current position in an opened asset.
         */
        virtual uint64_t GetAssetPosition(OpenRCT2::Platform::AssetHandle handle) = 0;

        /**
         * Seeks to a position in an opened asset.
         */
        virtual void SeekAsset(OpenRCT2::Platform::AssetHandle handle, int64_t offset, int32_t origin) = 0;

        /**
         * Reads data from an opened asset.
         */
        virtual uint64_t ReadAsset(OpenRCT2::Platform::AssetHandle handle, void* buffer, uint64_t length) = 0;

        /**
         * Tries to read data from an opened asset.
         */
        virtual uint64_t TryReadAsset(OpenRCT2::Platform::AssetHandle handle, void* buffer, uint64_t length) = 0;

        /**
         * Gets a list of all assets in the provider.
         */
        virtual const std::vector<AssetInfo>& GetAssetList() = 0;
    };
} // namespace OpenRCT2
