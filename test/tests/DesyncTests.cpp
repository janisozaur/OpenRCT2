/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/management/Research.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/scenario/Scenario.h>

using namespace OpenRCT2;

class DesyncTests : public testing::Test
{
protected:
    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        // Set data path so it can find objects/language if needed, though we try to avoid it for pure logic tests
        // gCustomOpenRCT2DataPath = "../data";
        _context->Initialise();
    }

    std::unique_ptr<IContext> _context;
};

TEST_F(DesyncTests, ScenarioRandIsDeterministic)
{
    ScenarioRandSeed(0x11223344, 0x55667788);
    auto val1 = ScenarioRand();
    auto val2 = ScenarioRand();

    ScenarioRandSeed(0x11223344, 0x55667788);
    ASSERT_EQ(val1, ScenarioRand());
    ASSERT_EQ(val2, ScenarioRand());
}

TEST_F(DesyncTests, ScenarioRandMaxIsDeterministic)
{
    ScenarioRandSeed(0xDEADBEEF, 0xCAFEBABE);
    auto val1 = ScenarioRandMax(100);
    auto val2 = ScenarioRandMax(1000);

    ScenarioRandSeed(0xDEADBEEF, 0xCAFEBABE);
    ASSERT_EQ(val1, ScenarioRandMax(100));
    ASSERT_EQ(val2, ScenarioRandMax(1000));

    ASSERT_LT(val1, 100u);
    ASSERT_LT(val2, 1000u);
}

TEST_F(DesyncTests, ResearchShuffleIsDeterministic)
{
    auto& gameState = getGameState();

    // Setup some uninvented items
    gameState.researchItemsUninvented.clear();
    for (int i = 0; i < 100; i++)
    {
        gameState.researchItemsUninvented.emplace_back(
            Research::EntryType::ride, static_cast<ObjectEntryIndex>(i), 0, ResearchCategory::gentle, 0);
    }

    auto originalList = gameState.researchItemsUninvented;

    // Seed and shuffle
    ScenarioRandSeed(0x12345678, 0x87654321);
    ResearchItemsShuffle();
    auto result1 = gameState.researchItemsUninvented;

    // Reset, seed with same value and shuffle again
    gameState.researchItemsUninvented = originalList;
    ScenarioRandSeed(0x12345678, 0x87654321);
    ResearchItemsShuffle();
    auto result2 = gameState.researchItemsUninvented;

    ASSERT_EQ(result1.size(), originalList.size());
    for (size_t i = 0; i < result1.size(); i++)
    {
        ASSERT_EQ(result1[i].entryIndex, result2[i].entryIndex);
    }

    // Verify it actually shuffled (low probability of being exactly the same)
    bool different = false;
    for (size_t i = 0; i < result1.size(); i++)
    {
        if (result1[i].entryIndex != originalList[i].entryIndex)
        {
            different = true;
            break;
        }
    }
    ASSERT_TRUE(different);
}
