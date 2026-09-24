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

TEST(FoveationTests, DefaultSettings)
{
    FoveatedRenderingSettings settings;
    EXPECT_FALSE(settings.enabled);
    EXPECT_FLOAT_EQ(settings.focalCenterX, 0.5f);
    EXPECT_FLOAT_EQ(settings.focalCenterY, 0.5f);
    EXPECT_FLOAT_EQ(settings.innerRadius, 200.0f);
    EXPECT_FLOAT_EQ(settings.outerRadius, 400.0f);
    EXPECT_EQ(settings.peripheralZoomOffset, 1);
}

TEST(FoveationTests, GetPeripheralZoomLevel)
{
    FoveatedRenderingSettings settings;
    settings.peripheralZoomOffset = 1;

    ZoomLevel baseZoom{ 0 };
    EXPECT_EQ(settings.GetPeripheralZoomLevel(baseZoom), ZoomLevel{ 1 });

    ZoomLevel maxZoom = ZoomLevel::max();
    EXPECT_EQ(settings.GetPeripheralZoomLevel(maxZoom), ZoomLevel::max());

    ZoomLevel minZoom = ZoomLevel::min();
    settings.peripheralZoomOffset = -5;
    EXPECT_EQ(settings.GetPeripheralZoomLevel(minZoom), ZoomLevel::min());

    settings.peripheralZoomOffset = 2;
    EXPECT_EQ(settings.GetPeripheralZoomLevel(ZoomLevel{ 0 }), ZoomLevel{ 2 });
}
