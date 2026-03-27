/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../../world/Location.hpp"

namespace OpenRCT2::GameActions
{
    struct LandSetHeightUpdate
    {
        CoordsXY Coords;
        uint8_t Height{};
        uint8_t Style{};
    };
} // namespace OpenRCT2::GameActions
