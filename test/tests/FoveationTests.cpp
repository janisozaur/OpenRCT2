/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/drawing/Foveation.h>

using namespace OpenRCT2::Drawing;

TEST(FoveationTests, GetPeripheralZoomLevel)
{
    FoveatedRenderingSettings settings;
    settings.peripheralZoomOffset = 1;

    ZoomLevel baseZoom{ 0 };
    EXPECT_EQ(settings.GetPeripheralZoomLevel(baseZoom), ZoomLevel{ 1 });

    ZoomLevel maxZoom = ZoomLevel::max();
    EXPECT_EQ(settings.GetPeripheralZoomLevel(maxZoom), ZoomLevel::max());

    settings.peripheralZoomOffset = 2;
    EXPECT_EQ(settings.GetPeripheralZoomLevel(ZoomLevel{ 0 }), ZoomLevel{ 2 });
}
