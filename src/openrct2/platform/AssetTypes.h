/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../core/StringTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace OpenRCT2
{
    namespace Platform
    {
        enum class AssetCheckResult
        {
            NotApplicable,
            Found,
            NotFound,
        };

        using AssetHandle = void*;

        struct AssetFileOpenResult
        {
            AssetCheckResult result;
            AssetHandle handle;
            uint64_t size;
        };
    }

    struct AssetInfo
    {
        std::string Path;
        uint64_t Size;
    };
} // namespace OpenRCT2
