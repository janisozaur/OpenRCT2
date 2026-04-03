/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
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
            name: 'test-plugin',
            version: '1.0.0',
            authors: ['test'],
            type: 'remote',
            licence: 'MIT',
            minApiVersion: 0,
            targetApiVersion: 0,
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

    // Trigger the event. interval.tick passes an object with { tick: number }
    // We can use HookEngine::Call directly or just call scriptEngine.Tick() if we want it more end-to-end
    // but interval.tick in HookEngine is called with an object.

    // Actually, HookEngine::Call(HookType::IntervalTick, ...) is what we want.
    // In ScriptEngine::UpdateIntervals, it doesn't seem to use HookEngine for intervals though.
    // Wait, HookType::IntervalTick is used in ScriptEngine?
    // Let's check HookEngine.cpp again for who calls interval.tick.

    // It seems interval.tick is not called by the core, but available.
    // Let's use an event that is actually called, like 'map.changed' or just manually call HookEngine::Call.

    auto& hookEngine = scriptEngine.GetHookEngine();

    // We need a JSValue to pass to Call.
    JSContext* ctx = scriptEngine.GetContext();
    JSValue arg = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, arg, "test", JS_NewInt32(ctx, 1));

    // This should NOT crash.
    // If it crashes, the test fails by crashing the process.
    hookEngine.Call(HookType::intervalTick, arg, false);
}

#endif
