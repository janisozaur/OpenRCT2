#include <string>
#include <gtest/gtest.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/Context.h>
#include <openrct2/core/File.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/core/String.hpp>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/ParkImporter.h>
#include "TestData.h"

extern "C"
{
    #include <openrct2/platform/platform.h>
    #include <openrct2/game.h>
}

using namespace OpenRCT2;

constexpr sint32 updatesToTest = 10;

TEST(MultiLaunchTest, all)
{
    std::string path = TestData::GetParkPath("bpb.sv6");

    gOpenRCT2Headless = true;

    core_init();
    for (int i = 0; i < 3; i++)
    {
        auto context = CreateContext();
        bool initialised = context->Initialise();
        ASSERT_TRUE(initialised);

        ParkLoadResult * plr = load_from_sv6(path.c_str());

        ASSERT_EQ(ParkLoadResult_GetError(plr), PARK_LOAD_ERROR_OK);
        ParkLoadResult_Delete(plr);

        game_load_init();

        // Check ride count to check load was successful
        ASSERT_EQ(gRideCount, 134);
        auto gs = context->GetGameState();
        ASSERT_NE(gs, nullptr);
        auto date = gs->GetDate();
        ASSERT_EQ(date->GetMonthTicks(), 0);

        for (int j = 0; j < updatesToTest; j++)
        {
            gs->UpdateLogic();
        }

        ASSERT_EQ(date->GetMonthTicks(), 7862 + updatesToTest);

        // Check ride count again
        ASSERT_EQ(gRideCount, 134);

        delete context;
    }
    SUCCEED();
}
