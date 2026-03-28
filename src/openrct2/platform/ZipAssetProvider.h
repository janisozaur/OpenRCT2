/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "IAssetProvider.h"

#include <memory>
#include <string>

namespace OpenRCT2
{
    /**
     * Creates an asset provider that reads from a ZIP archive.
     */
    std::unique_ptr<IAssetProvider> CreateZipAssetProvider(const std::string& zipPath);
} // namespace OpenRCT2
