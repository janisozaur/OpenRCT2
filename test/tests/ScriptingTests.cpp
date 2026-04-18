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
#include <openrct2/scripting/ScriptEngine.h>
#include <quickjs.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Scripting;

class ScriptingTests : public testing::Test
{
protected:
    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        _context->Initialise();
    }

    std::unique_ptr<IContext> _context;
};

#ifdef ENABLE_SCRIPTING

TEST_F(ScriptingTests, MultipleSubscribersToSameEventShouldNotCrash)
{
    auto& scriptEngine = static_cast<ScriptEngine&>(_context->GetScriptEngine());

    // Register a plugin that subscribes twice to the same event
    const char* pluginCode = R"(
        registerPlugin({
            name: 'test-plugin-multiple-subscribers',
            version: '1.0.0',
            authors: ['openrct2-test'],
            type: 'remote',
            licence: 'MIT',
            minApiVersion: 110, // deliberately the version before quickjs
            targetApiVersion: 110,
            main: function () {
                context.subscribe('interval.tick', function (e) {
                    // first subscriber
                });
                context.subscribe('interval.tick', function (e) {
                    // second subscriber
                });
            }
        });
    )";

    scriptEngine.AddNetworkPlugin(pluginCode);
    scriptEngine.LoadTransientPlugins();
    scriptEngine.Tick();

    auto& hookEngine = scriptEngine.GetHookEngine();

    // We need a JSValue to pass to Call.
    JSContext* ctx = scriptEngine.GetContext();
    JSValue arg = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, arg, "test", JS_NewInt32(ctx, 1));

    // This should NOT crash.
    hookEngine.Call(HookType::intervalTick, arg, false);
}

TEST_F(ScriptingTests, RideItemsSoldProperties)
{
    auto& scriptEngine = static_cast<ScriptEngine&>(_context->GetScriptEngine());

    // Allocate a ride
    RideId rideId = RideId::FromUnderlying(0);
    Ride* ride = RideAllocateAtIndex(rideId);
    ASSERT_NE(ride, nullptr);

    ride->type = 28; // Food stall
    ride->numPrimaryItemsSold = 123;
    ride->numSecondaryItemsSold = 456;

    // We don't have objects loaded, so primaryItem/secondaryItem might be "none" or null.
    // That's okay, we just want to see if the properties exist and return something sensible.

    const char* pluginCode = R"(
        registerPlugin({
            name: 'test-plugin-ride-stats',
            version: '1.0.0',
            authors: ['openrct2-test'],
            type: 'remote',
            licence: 'MIT',
            minApiVersion: 110,
            targetApiVersion: 110,
            main: function () {
                var ride = map.getRide(0);
                context.sharedStorage.set('test.numPrimary', ride.numPrimaryItemsSold);
                context.sharedStorage.set('test.numSecondary', ride.numSecondaryItemsSold);
                context.sharedStorage.set('test.primaryItem', ride.primaryItem);
                context.sharedStorage.set('test.secondaryItem', ride.secondaryItem);
            }
        });
    )";

    scriptEngine.AddNetworkPlugin(pluginCode);
    scriptEngine.LoadTransientPlugins();
    scriptEngine.Tick();

    JSContext* ctx = scriptEngine.GetContext();
    auto getSharedStorage = [&](const char* key) -> JSValue {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue sharedStorage = JS_GetPropertyStr(ctx, global, "context");
        JSValue sharedStorageObj = JS_GetPropertyStr(ctx, sharedStorage, "sharedStorage");
        JSValue getFunc = JS_GetPropertyStr(ctx, sharedStorageObj, "get");
        JSValue keyVal = JS_NewString(ctx, key);
        JSValue result = JS_Call(ctx, getFunc, sharedStorageObj, 1, &keyVal);
        JS_FreeValue(ctx, keyVal);
        JS_FreeValue(ctx, getFunc);
        JS_FreeValue(ctx, sharedStorageObj);
        JS_FreeValue(ctx, sharedStorage);
        JS_FreeValue(ctx, global);
        return result;
    };

    JSValue numPrimary = getSharedStorage("test.numPrimary");
    JSValue numSecondary = getSharedStorage("test.numSecondary");
    JSValue primaryItem = getSharedStorage("test.primaryItem");
    JSValue secondaryItem = getSharedStorage("test.secondaryItem");

    int32_t valPrimary;
    JS_ToInt32(ctx, &valPrimary, numPrimary);
    EXPECT_EQ(valPrimary, 123);

    int32_t valSecondary;
    JS_ToInt32(ctx, &valSecondary, numSecondary);
    EXPECT_EQ(valSecondary, 456);

    EXPECT_TRUE(JS_IsNull(primaryItem) || JS_IsString(primaryItem));
    EXPECT_TRUE(JS_IsNull(secondaryItem) || JS_IsString(secondaryItem));

    JS_FreeValue(ctx, numPrimary);
    JS_FreeValue(ctx, numSecondary);
    JS_FreeValue(ctx, primaryItem);
    JS_FreeValue(ctx, secondaryItem);
}

#endif
