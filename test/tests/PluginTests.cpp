/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TestData.h"

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/ParkImporter.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/object/ObjectList.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/platform/Platform.h>
#include <openrct2/ride/RideManager.hpp>
#include <openrct2/scripting/Plugin.h>
#include <openrct2/scripting/ScriptEngine.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/Park.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/TrackElement.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Scripting;

class PluginTests : public testing::Test
{
protected:
    std::unique_ptr<IContext> _context;

    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        gCustomUserDataPath = TestData::GetBasePath();

        _context = CreateContext();
        ASSERT_TRUE(_context->Initialise());

        // Register test.assert bridge
        auto& engine = _context->GetScriptEngine();
        auto* ctx = engine.GetContext();

        dukglue_register_function(
            ctx, +[](bool condition, std::string message) { EXPECT_TRUE(condition) << message; }, "test_assert");
        duk_peval_string(ctx, "var test = { assert: test_assert };");
    }

    void TearDown() override
    {
        if (_context)
        {
            _context->GetScriptEngine().StopUnloadRegisterAllPlugins();
            _context.reset();
        }
    }

    void LoadPark(const std::string& parkName)
    {
        auto path = TestData::GetParkPath(parkName);
        auto importer = ParkImporter::CreateS6(_context->GetObjectRepository());
        auto loadResult = importer->LoadSavedGame(path.c_str(), false);
        _context->GetObjectManager().LoadObjects(loadResult.RequiredObjects);

        // Load some additional objects for testing
        ObjectList additionalObjects;
        additionalObjects.Add(ObjectEntryDescriptor(ObjectType::ride, "rct2.ride.ptct2"));
        additionalObjects.Add(ObjectEntryDescriptor(ObjectType::ride, "rct2.ride.mgr1"));
        _context->GetObjectManager().LoadObjects(additionalObjects);

        auto& gameState = getGameState();
        importer->Import(gameState);
        gameState.park.flags |= PARK_FLAGS_UNLOCK_ALL_PRICES;
        gameState.cheats.ignoreResearchStatus = true;
    }

    void LoadPlugin(const std::string& filename)
    {
        auto path = TestData::GetPluginsPath();
        path = Path::Combine(path, filename);
        auto& engine = _context->GetScriptEngine();

        engine.RegisterPlugin(path);

        // Find the plugin in the list
        auto& plugins = engine.GetPlugins();
        auto it = std::find_if(
            plugins.begin(), plugins.end(), [&](const std::shared_ptr<Plugin>& p) { return p->GetPath() == path; });
        ASSERT_NE(it, plugins.end()) << "Plugin not found at " << path;

        engine.LoadPlugin(*it);
        engine.StartPlugin(*it);

        // Execute queued actions
        GameActions::ProcessQueue(getGameState());
    }
};

TEST_F(PluginTests, BasicPluginLoading)
{
    LoadPlugin("basic_test.js");

    auto& engine = _context->GetScriptEngine();
    auto& plugins = engine.GetPlugins();
    ASSERT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0]->GetMetadata().Name, "Basic Test Plugin");
}

TEST_F(PluginTests, ActionExecution)
{
    LoadPark("small_park_with_ferris_wheel.sv6");

    // Set some initial state
    auto& gameState = getGameState();
    gameState.park.name = "Initial Name";
    gameState.park.entranceFee = 0;

    LoadPlugin("action_test.js");

    // Plugin runs actions in its main()
    EXPECT_EQ(gameState.park.name, "Test Park");
    EXPECT_EQ(gameState.park.entranceFee, 123);
}

TEST_F(PluginTests, HookSubscription)
{
    LoadPark("small_park_with_ferris_wheel.sv6");
    LoadPlugin("hook_test.js");

    // The plugin itself executes an action in main() and verifies the hook fires.
}

TEST_F(PluginTests, AsyncIntervals)
{
    LoadPark("small_park_with_ferris_wheel.sv6");
    LoadPlugin("async_test.js");

    auto& engine = _context->GetScriptEngine();

    duk_push_object(engine.GetContext());
    DukValue args = DukValue::take_from_stack(engine.GetContext());
    auto checkAction = engine.CreateGameAction("checkTimeout", args, "TestRunner");

    // Advance time. 100ms in JS.
    Platform::Sleep(200);
    engine.Tick();

    GameActions::Execute(checkAction.get(), getGameState());
}

TEST_F(PluginTests, PluginTypes)
{
    LoadPark("small_park_with_ferris_wheel.sv6");
    LoadPlugin("plugin_types.js");

    auto& engine = _context->GetScriptEngine();
    auto& plugins = engine.GetPlugins();
    ASSERT_EQ(plugins.size(), 1);
    EXPECT_EQ(plugins[0]->GetMetadata().Type, PluginType::Intransient);

    duk_push_object(engine.GetContext());
    DukValue args = DukValue::take_from_stack(engine.GetContext());
    auto checkAction = engine.CreateGameAction("checkType", args, "TestRunner");

    gInUpdateCode = true;
    auto res = GameActions::Execute(checkAction.get(), getGameState());
    gInUpdateCode = false;
    EXPECT_EQ(res.cost, 42);
}

TEST_F(PluginTests, RideConstruction)
{
    // Initialize empty map
    auto& gameState = getGameState();
    gameStateInitAll(gameState, { 64, 64 });
    for (int32_t y = 0; y < 64; y++)
    {
        for (int32_t x = 0; x < 64; x++)
        {
            auto* surface = MapGetSurfaceElementAt(TileCoordsXY(x, y));
            if (surface)
                surface->SetOwnership(OWNERSHIP_OWNED);
        }
    }
    gameState.park.flags |= PARK_FLAGS_UNLOCK_ALL_PRICES | PARK_FLAGS_NO_MONEY;
    gameState.cheats.ignoreResearchStatus = true;
    gameState.cheats.showAllOperatingModes = true;

    // Load necessary objects
    ObjectList additionalObjects;
    additionalObjects.Add(ObjectEntryDescriptor(ObjectType::ride, "rct2.ride.mgr1"));
    _context->GetObjectManager().LoadObjects(additionalObjects);

    LoadPlugin("ride_construction.js");

    auto rideManager = RideManager(gameState);

    // Find our Carousel
    Ride* ride = nullptr;
    for (auto& r : rideManager)
    {
        if (r.type == RIDE_TYPE_MERRY_GO_ROUND)
        {
            ride = &r;
            break;
        }
    }

    ASSERT_NE(ride, nullptr);
    EXPECT_EQ(ride->mode, RideMode::singleRidePerAdmission); // Mode 9

    // Check track pieces - we built 1 carousel piece
    int trackCount = 0;
    TileElementIterator it;
    TileElementIteratorBegin(&it);
    while (TileElementIteratorNext(&it))
    {
        if (it.element->GetType() == TileElementType::Track)
        {
            auto* trackElement = it.element->as<TrackElement>();
            if (trackElement->GetRideIndex() == ride->id)
            {
                trackCount++;
            }
        }
    }
    EXPECT_EQ(trackCount, 1);
}
