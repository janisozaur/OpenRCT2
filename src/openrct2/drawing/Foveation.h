/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../interface/ScreenCoords.hpp"
#include "../interface/ZoomLevel.h"

namespace OpenRCT2::Drawing
{
    /**
     * Settings representing foveated rendering parameters.
     * Follows patterns aligned with OpenXR XR_FB_foveation specifications
     * (e.g. normalized focal center coordinates and focal gains/radii).
     */
    struct FoveatedRenderingSettings
    {
        bool enabled{ false };

        // Normalized focal center coordinates [0.0, 1.0] relative to screen/viewport
        float focalCenterX{ 0.5f };
        float focalCenterY{ 0.5f };

        // Focal area radius in screen pixels
        float innerRadius{ 200.0f };
        float outerRadius{ 400.0f };

        // Scale factors / gains (1.0 = normal, >1.0 = faster peripheral falloff)
        float gainX{ 1.0f };
        float gainY{ 1.0f };

        // Zoom level offset applied for peripheral vision areas outside focal radius
        int8_t peripheralZoomOffset{ 1 };

        [[nodiscard]] ZoomLevel GetPeripheralZoomLevel(ZoomLevel baseZoom) const
        {
            int32_t targetZoom = static_cast<int32_t>(static_cast<int8_t>(baseZoom)) + peripheralZoomOffset;
            if (targetZoom < static_cast<int32_t>(static_cast<int8_t>(ZoomLevel::min())))
                return ZoomLevel::min();
            if (targetZoom > static_cast<int32_t>(static_cast<int8_t>(ZoomLevel::max())))
                return ZoomLevel::max();
            return ZoomLevel(static_cast<int8_t>(targetZoom));
        }
    };

    extern FoveatedRenderingSettings gFoveatedRenderingSettings;
    extern bool gFoveationFollowsCursor;
} // namespace OpenRCT2::Drawing
