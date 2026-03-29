/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TestData.h"

#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/terraform/LandSetHeightBulkAction.h>
#include <openrct2/world/Map.h>

using namespace OpenRCT2;
using namespace OpenRCT2::GameActions;

class BulkActionTests : public testing::Test
{
protected:
    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        _context->Initialise();

        // Create a small empty map for testing
        MapInit({ 64, 64 });
        GameLoadInit();
    }

    void TearDown() override
    {
        _context.reset();
    }

private:
    std::shared_ptr<IContext> _context;
};

TEST_F(BulkActionTests, LandSetHeightBulkAction)
{
    std::vector<LandSetHeightUpdate> updates;
    updates.push_back({ { 32 * 32, 32 * 32 }, 10, 0 });
    updates.push_back({ { 33 * 32, 32 * 32 }, 12, 5 });

    LandSetHeightBulkAction action(updates);
    auto result = action.Execute(getGameState());

    ASSERT_EQ(result.error, Status::ok);

    auto surface1 = MapGetSurfaceElementAt(CoordsXY{ 32 * 32, 32 * 32 });
    ASSERT_NE(surface1, nullptr);
    EXPECT_EQ(surface1->GetBaseHeight(), 10);
    EXPECT_EQ(surface1->GetSlope(), 0);

    auto surface2 = MapGetSurfaceElementAt(CoordsXY{ 33 * 32, 32 * 32 });
    ASSERT_NE(surface2, nullptr);
    EXPECT_EQ(surface2->GetBaseHeight(), 12);
    EXPECT_EQ(surface2->GetSlope(), 5);
}
