/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/RideObject.h>
#include <openrct2/scripting/ScriptEngine.h>
#include <openrct2/world/Park.h>
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
        gCustomOpenRCT2DataPath = "../data";
        _context = CreateContext();
        bool initialised = _context->Initialise();
        ASSERT_TRUE(initialised);

        // Load an empty park or initialize game state
        GameLoadInit();
    }

    void TearDown() override
    {
        _context.reset();
    }

    ScriptEngine& GetScriptEngine()
    {
        return _context->GetScriptEngine();
    }

    std::shared_ptr<IContext> _context;
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

TEST_F(ScriptingTests, RideObjectPrototypeChain)
{
    auto& engine = GetScriptEngine();
    JSContext* ctx = engine.GetContext();

    // Find a loaded ride object
    auto& objManager = _context->GetObjectManager();
    const auto* rideObj = objManager.GetLoadedObject<RideObject>(0);
    if (rideObj == nullptr)
    {
        GTEST_SKIP() << "Couldn't find usable ride for test";
        return;
    }

    std::string script = "var rideObj = objectManager.getObject('ride', 0);";
    script += "var test_res_name = rideObj.name;";         // from ScObject
    script += "var test_res_capacity = rideObj.capacity;"; // from ScRideObject

    JSValue res = JS_Eval(ctx, script.c_str(), script.length(), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(res))
    {
        JSValue exp = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exp);
        fprintf(stderr, "JS Exception: %s\n", str);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, exp);
    }
    JS_FreeValue(ctx, res);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue nameVal = JS_GetPropertyStr(ctx, global, "test_res_name");
    JSValue capacityVal = JS_GetPropertyStr(ctx, global, "test_res_capacity");

    const char* nameStr = JS_ToCString(ctx, nameVal);
    EXPECT_STREQ(nameStr, rideObj->GetName().c_str());
    JS_FreeCString(ctx, nameStr);

    const char* capacityStr = JS_ToCString(ctx, capacityVal);
    EXPECT_STREQ(capacityStr, rideObj->GetCapacity().c_str());
    JS_FreeCString(ctx, capacityStr);

    JS_FreeValue(ctx, nameVal);
    JS_FreeValue(ctx, capacityVal);
    JS_FreeValue(ctx, global);
}

TEST_F(ScriptingTests, GuestPrototypeChain)
{
    auto& engine = GetScriptEngine();
    JSContext* ctx = engine.GetContext();

    // Directly execute JS to verify properties are set correctly through the prototype chain
    std::string script = "var guest = map.createEntity('steam_particle', {x: 320, y: 480, z: 2});";
    script += "var test_guest_found = (guest !== undefined);";
    script += "if (guest !== undefined) {";
    script += "  guest.energy = 50;"; // from ScPeep (steam_particle is ScEntity, but let's try a real peep type if possible)
    script += "  var test_guest_id = guest.id;"; // from ScEntity
    script += "}";

    // Steam particle is ScEntity. Guest is ScGuest -> ScPeep -> ScEntity.
    // Let's use map.createEntity('guest') again but with more care.
    script = "var guest = map.createEntity('guest', {x: 320, y: 480, z: 2});";
    script += "var test_guest_found = (guest !== undefined);";
    script += "if (guest !== undefined) {";
    script += "  guest.energy = 50;";
    script += "  guest.happiness = 123;";
    script += "  var test_guest_energy = guest.energy;";
    script += "  var test_guest_happiness = guest.happiness;";
    script += "}";

    JSValue res = JS_Eval(ctx, script.c_str(), script.length(), "<test>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, res);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue foundVal = JS_GetPropertyStr(ctx, global, "test_guest_found");
    if (JS_ToBool(ctx, foundVal))
    {
        JSValue energyVal = JS_GetPropertyStr(ctx, global, "test_guest_energy");
        int32_t energy;
        JS_ToInt32(ctx, &energy, energyVal);
        EXPECT_EQ(energy, 50);
        JS_FreeValue(ctx, energyVal);

        JSValue happinessVal = JS_GetPropertyStr(ctx, global, "test_guest_happiness");
        int32_t happiness;
        JS_ToInt32(ctx, &happiness, happinessVal);
        EXPECT_EQ(happiness, 123);
        JS_FreeValue(ctx, happinessVal);
    }
    JS_FreeValue(ctx, foundVal);
    JS_FreeValue(ctx, global);
}

TEST_F(ScriptingTests, StaffPrototypeChain)
{
    auto& engine = GetScriptEngine();
    JSContext* ctx = engine.GetContext();

    // Create a staff member via map.createEntity. Returns ScStaff wrapper.
    // Then set its type to handyman.
    std::string script = "var staff = map.createEntity('staff', {x: 10, y: 10, z: 2});";
    script += "var test_staff_found = (staff !== undefined);";
    script += "if (staff !== undefined) {";
    script += "  staff.staffType = 'handyman';"; // from ScStaff
    script += "  var handyman = map.getEntity(staff.id);";
    script += "  var test_handyman_found = (handyman !== null);";
    script += "  if (handyman !== null) {";
    script += "    handyman.litterSwept = 42;"; // from ScHandyman
    script += "    var test_handyman_swept = handyman.litterSwept;";
    script += "  }";
    script += "}";

    JSValue res = JS_Eval(ctx, script.c_str(), script.length(), "<test>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(ctx, res);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue foundVal = JS_GetPropertyStr(ctx, global, "test_handyman_found");
    if (JS_ToBool(ctx, foundVal))
    {
        JSValue sweptVal = JS_GetPropertyStr(ctx, global, "test_handyman_swept");
        int32_t swept;
        JS_ToInt32(ctx, &swept, sweptVal);
        EXPECT_EQ(swept, 42);
        JS_FreeValue(ctx, sweptVal);
    }
    JS_FreeValue(ctx, foundVal);
    JS_FreeValue(ctx, global);
}

TEST_F(ScriptingTests, ScenarioPrototypeChain)
{
    auto& engine = GetScriptEngine();
    JSContext* ctx = engine.GetContext();

    getGameState().scenarioOptions.name = "My Scenario";
    getGameState().scenarioOptions.objective.Type = OpenRCT2::Scenario::ObjectiveType::guestsBy;
    getGameState().scenarioOptions.objective.NumGuests = 500;

    std::string script = "var sc = scenario;";
    script += "var obj = sc.objective;";
    script += "var test_sc_name = sc.name;";
    script += "var test_obj_type = obj.type;";
    script += "var test_obj_guests = obj.guests;";
    script += "sc.name = 'New Name';";
    script += "obj.guests = 1000;";

    JSValue res = JS_Eval(ctx, script.c_str(), script.length(), "<test>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(res))
    {
        JSValue exp = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exp);
        fprintf(stderr, "JS Exception: %s\n", str);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, exp);
    }
    JS_FreeValue(ctx, res);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue nameVal = JS_GetPropertyStr(ctx, global, "test_sc_name");
    const char* nameStr = JS_ToCString(ctx, nameVal);
    EXPECT_STREQ(nameStr, "My Scenario");
    JS_FreeCString(ctx, nameStr);
    JS_FreeValue(ctx, nameVal);

    JSValue guestsVal = JS_GetPropertyStr(ctx, global, "test_obj_guests");
    int32_t guests;
    JS_ToInt32(ctx, &guests, guestsVal);
    EXPECT_EQ(guests, 500);
    JS_FreeValue(ctx, guestsVal);

    EXPECT_STREQ(getGameState().scenarioOptions.name.c_str(), "New Name");
    EXPECT_EQ(getGameState().scenarioOptions.objective.NumGuests, 1000);

    JS_FreeValue(ctx, global);
}
#endif
