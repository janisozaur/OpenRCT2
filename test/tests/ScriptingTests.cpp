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

TEST_F(ScriptingTests, Base64AndZstdAPIs)
{
    auto& scriptEngine = static_cast<ScriptEngine&>(_context->GetScriptEngine());

    const char* pluginCode = R"(
        registerPlugin({
            name: 'test-plugin-crypto',
            version: '1.0.0',
            authors: ['openrct2-test'],
            type: 'remote',
            licence: 'MIT',
            main: function () {
                // Base64 Test
                const originalData = new Uint8Array([72, 101, 108, 108, 111]); // "Hello"
                const encoded = context.base64encode(originalData);
                if (encoded.data !== "SGVsbG8=") {
                    throw new Error("Base64 encode failed: " + encoded.data);
                }
                const decoded = context.base64decode(encoded.data);
                if (decoded.data.length !== 5 || decoded.data[0] !== 72 || decoded.data[4] !== 111) {
                    throw new Error("Base64 decode failed");
                }

                // Zstd Test
                const largeData = new Uint8Array(100).fill(65); // 100 'A's
                const compressed = context.zstdCompress(largeData);
                if (compressed.error) {
                    throw new Error("Zstd compress failed: " + compressed.message);
                }
                const decompressed = context.zstdDecompress(compressed.data);
                if (decompressed.error) {
                    throw new Error("Zstd decompress failed: " + decompressed.message);
                }
                if (decompressed.data.length !== 100 || decompressed.data[0] !== 65) {
                    throw new Error("Zstd decompression data mismatch");
                }

                // Zstd Test with explicit length
                const decompressed2 = context.zstdDecompress(compressed.data, 100);
                if (decompressed2.data.length !== 100) {
                    throw new Error("Zstd decompression with explicit length failed");
                }

                // Zstd Test with smaller length
                const decompressed3 = context.zstdDecompress(compressed.data, 50);
                if (decompressed3.data.length !== 50 || !decompressed3.moreAvailable) {
                    throw new Error("Zstd decompression with smaller length failed");
                }

                console.log("Crypto tests passed!");
            }
        });
    )";

    scriptEngine.AddNetworkPlugin(pluginCode);
    scriptEngine.LoadTransientPlugins();
    scriptEngine.Tick();
}

#endif
