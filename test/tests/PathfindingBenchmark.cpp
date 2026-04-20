#include "TestData.h"

#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/entity/Guest.h>
#include <openrct2/peep/GuestPathfinding.h>
#include <openrct2/ride/RideManager.hpp>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/world/Map.h>

using namespace OpenRCT2;

class PathfindingBenchmark : public testing::Test
{
public:
    static void SetUpTestCase()
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        _context->Initialise();

        std::string parkPath = TestData::GetParkPath("pathfinding-tests.sv6");
        GetContext()->LoadParkFromFile(parkPath);
        GameLoadInit();
    }

    static void TearDownTestCase()
    {
        _context = nullptr;
    }

protected:
    static Ride* FindRideByName(const char* name)
    {
        auto& gameState = getGameState();
        for (auto& ride : RideManager(gameState))
        {
            if (ride.getName().find(name) != std::string::npos)
            {
                return &ride;
            }
        }
        return nullptr;
    }

private:
    static std::shared_ptr<IContext> _context;
};

std::shared_ptr<IContext> PathfindingBenchmark::_context;

TEST_F(PathfindingBenchmark, ChooseDirectionBenchmark)
{
    struct Scenario
    {
        const char* name;
        TileCoordsXYZ start;
    };

    std::vector<Scenario> scenarios = { { "StraightFlat", { 19, 15, 14 } },
                                        { "SBend", { 15, 12, 14 } },
                                        { "UBend", { 17, 9, 14 } },
                                        { "CBend", { 14, 5, 14 } },
                                        { "TwoEqualRoutes", { 9, 13, 14 } },
                                        { "TwoUnequalRoutes", { 3, 13, 14 } },
                                        { "StraightUpBridge", { 12, 15, 14 } },
                                        { "StraightUpSlope", { 14, 15, 14 } },
                                        { "SelfCrossingPath", { 6, 5, 14 } } };

    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    int totalCalls = 0;
    for (int i = 0; i < iterations; ++i)
    {
        for (const auto& scenario : scenarios)
        {
            auto ride = FindRideByName(scenario.name);
            if (!ride)
                continue;

            auto entrancePos = ride->getStation().Entrance;
            TileCoordsXYZ goal = TileCoordsXYZ(
                entrancePos.x - TileDirectionDelta[entrancePos.direction].x,
                entrancePos.y - TileDirectionDelta[entrancePos.direction].y, entrancePos.z);

            auto* peep = Guest::Generate(scenario.start.ToCoordsXYZ().ToTileCentre());
            peep->OutsideOfPark = false;
            peep->GuestHeadingToRideId = ride->id;

            PathFinding::ChooseDirection(scenario.start, goal, *peep, false, RideId::GetNull());

            PeepEntityRemove(peep);
            totalCalls++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "[          ] Benchmark: " << totalCalls << " calls to ChooseDirection took " << duration.count() << " us ("
              << (duration.count() / totalCalls) << " us/call)" << std::endl;
}
