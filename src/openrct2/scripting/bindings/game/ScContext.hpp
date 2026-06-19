/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifdef ENABLE_SCRIPTING

    #include "../../../../thirdparty/base64.hpp"
    #include "../../../Game.h"
    #include "../../../OpenRCT2.h"
    #include "../../../actions/GameActionRunner.h"
    #include "../../../core/Compression.h"
    #include "../../../core/MemoryStream.h"
    #include "../../../interface/Screenshot.h"
    #include "../../../localisation/Formatting.h"
    #include "../../../object/ObjectManager.h"
    #include "../../../scenario/Scenario.h"
    #include "../../HookEngine.h"
    #include "../../IconNames.hpp"
    #include "../../ScriptEngine.h"
    #include "../game/ScConfiguration.hpp"
    #include "../game/ScDisposable.hpp"
    #include "../object/ScObjectManager.h"
    #include "../ride/ScTrackSegment.h"

    #include <cstdio>
    #include <memory>
    #include <zstd.h>

namespace OpenRCT2::Scripting
{
    class ScContext;
    extern ScContext gScContext;

    class ScContext final : public ScBase
    {
    private:
        static JSValue apiVersion_get(JSContext* ctx, JSValue thisVal)
        {
            return JS_NewInt32(ctx, kPluginApiVersion);
        }

        static JSValue configuration_get(JSContext* ctx, JSValue thisVal)
        {
            return gScConfiguration.New(ctx);
        }

        static JSValue sharedStorage_get(JSContext* ctx, JSValue thisVal)
        {
            return gScConfiguration.New(ctx, ScConfigurationKind::Shared);
        }

        static JSValue getParkStorage(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JSValue jsPluginName = argv[0];

            auto& scriptEngine = GetContext()->GetScriptEngine();

            JSValue result = JS_UNDEFINED;
            if (JS_IsString(jsPluginName))
            {
                std::string pluginName = JSToStdString(ctx, jsPluginName);
                if (pluginName.empty())
                {
                    return JS_ThrowPlainError(ctx, "Plugin name is empty");
                }
                result = gScConfiguration.New(ctx, ScConfigurationKind::Park, pluginName);
            }
            else if (JS_IsUndefined(jsPluginName))
            {
                const auto& plugin = scriptEngine.GetExecInfo().GetCurrentPlugin();
                if (plugin == nullptr)
                {
                    return JS_ThrowPlainError(ctx, "Plugin name must be specified when used from console.");
                }
                result = gScConfiguration.New(ctx, ScConfigurationKind::Park, plugin->GetMetadata().Name);
            }
            else
            {
                return JS_ThrowPlainError(ctx, "Invalid plugin name.");
            }
            return result;
        }

        static JSValue mode_get(JSContext* ctx, JSValue thisVal)
        {
            if (gLegacyScene == LegacyScene::titleSequence)
                return JSFromStdString(ctx, "title");
            else if (gLegacyScene == LegacyScene::scenarioEditor)
                return JSFromStdString(ctx, "scenario_editor");
            else if (gLegacyScene == LegacyScene::trackDesigner)
                return JSFromStdString(ctx, "track_designer");
            else if (gLegacyScene == LegacyScene::trackDesignsManager)
                return JSFromStdString(ctx, "track_manager");
            return JSFromStdString(ctx, "normal");
        }

        static JSValue paused_get(JSContext* ctx, JSValue thisVal)
        {
            return JS_NewBool(ctx, GameIsPaused());
        }

        static JSValue paused_set(JSContext* ctx, JSValue thisVal, JSValue value)
        {
            JS_UNPACK_BOOL(valueBool, ctx, value)
            JS_THROW_IF_GAME_STATE_NOT_MUTABLE();

            if (valueBool != GameIsPaused())
                PauseToggle();

            return JS_UNDEFINED;
        }

        static JSValue captureImage(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JSValue options = argv[0];
            try
            {
                auto rotation = JSToOptionalInt(ctx, options, "rotation");
                auto zoom = JSToOptionalInt(ctx, options, "zoom");
                if (!rotation.has_value() || !zoom.has_value())
                {
                    JS_ThrowPlainError(ctx, "Invalid options.");
                    return JS_EXCEPTION;
                }

                CaptureOptions captureOptions;
                captureOptions.Filename = fs::u8path(AsOrDefault(ctx, options, "filename", ""));
                captureOptions.Rotation = rotation.value() & 3;
                captureOptions.Zoom = ZoomLevel(zoom.value());
                captureOptions.Transparent = AsOrDefault(ctx, options, "transparent", false);

                JSValue jsPosition = JS_GetPropertyStr(ctx, options, "position");
                if (JS_IsObject(jsPosition))
                {
                    auto width = JSToOptionalInt(ctx, options, "width");
                    auto height = JSToOptionalInt(ctx, options, "height");
                    auto x = JSToOptionalInt(ctx, jsPosition, "x");
                    auto y = JSToOptionalInt(ctx, jsPosition, "y");

                    if (!width.has_value() || !height.has_value() || !x.has_value() || !y.has_value())
                    {
                        JS_FreeValue(ctx, jsPosition);
                        JS_ThrowPlainError(ctx, "Invalid options.");
                        return JS_EXCEPTION;
                    }

                    CaptureView view;
                    view.Width = width.value();
                    view.Height = height.value();
                    view.Position.x = x.value();
                    view.Position.y = y.value();
                    captureOptions.View = view;
                }
                JS_FreeValue(ctx, jsPosition);

                CaptureImage(captureOptions);
            }
            catch (const std::exception& ex)
            {
                JS_ThrowPlainError(ctx, "%s", ex.what());
                return JS_EXCEPTION;
            }

            return JS_UNDEFINED;
        }

        static JSValue getObject(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            // deprecated function, moved to ObjectManager.getObject.
            return gScObjectManager.getObject(ctx, thisVal, argc, argv);
        }

        static JSValue getAllObjects(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            // deprecated function, moved to ObjectManager.getAllObjects.
            return gScObjectManager.getAllObjects(ctx, thisVal, argc, argv);
        }

        static JSValue getTrackSegment(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_INT32(type, ctx, argv[0])

            if (type >= EnumValue(TrackElemType::count))
            {
                return JS_NULL;
            }
            else
            {
                return gScTrackSegment.New(ctx, static_cast<TrackElemType>(type));
            }
        }

        static JSValue getAllTrackSegments(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            auto result = JS_NewArray(ctx);
            int64_t index = 0;
            for (uint16_t type = 0; type < EnumValue(TrackElemType::count); type++)
            {
                auto obj = gScTrackSegment.New(ctx, static_cast<TrackElemType>(type));
                JS_SetPropertyInt64(ctx, result, index++, obj);
            }
            return result;
        }

        static JSValue getRandom(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_INT32(min, ctx, argv[0]);
            JS_UNPACK_INT32(max, ctx, argv[1]);
            JS_THROW_IF_GAME_STATE_NOT_MUTABLE();

            if (min >= max)
                return JS_NewInt32(ctx, min);
            int32_t range = max - min;
            return JS_NewInt64(ctx, min + ScenarioRandMax(range));
        }

        static JSValue formatString(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            if (argc >= 1)
            {
                const JSValue jsFmt = argv[0];
                if (JS_IsString(jsFmt))
                {
                    FmtString fmt(JSToStdString(ctx, jsFmt));

                    std::vector<FormatArg_t> args;
                    for (int i = 1; i < argc; i++)
                    {
                        const JSValue jsArg = argv[i];
                        if (JS_IsNumber(jsArg))
                        {
                            args.emplace_back(JSToInt(ctx, jsArg));
                        }
                        else if (JS_IsString(jsArg))
                        {
                            args.emplace_back(JSToStdString(ctx, jsArg));
                        }
                        else
                        {
                            JS_ThrowPlainError(ctx, "Invalid format argument.");
                            return JS_EXCEPTION;
                        }
                    }

                    auto result = FormatStringAny(fmt, args);
                    return JSFromStdString(ctx, result);
                }
                else
                {
                    JS_ThrowPlainError(ctx, "Invalid format string.");
                    return JS_EXCEPTION;
                }
            }
            JS_ThrowPlainError(ctx, "Invalid format string.");
            return JS_EXCEPTION;
        }

        static JSValue subscribe(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(hook, ctx, argv[0]);
            JS_UNPACK_CALLBACK(callback, ctx, argv[1]);

            auto& scriptEngine = GetContext()->GetScriptEngine();

            auto hookType = GetHookType(hook);
            if (hookType == HookType::notDefined)
            {
                JS_ThrowPlainError(ctx, "Unknown hook type");
                return JS_EXCEPTION;
            }

            auto owner = scriptEngine.GetExecInfo().GetCurrentPlugin();
            if (owner == nullptr)
            {
                JS_ThrowPlainError(ctx, "Not in a plugin context");
                return JS_EXCEPTION;
            }

            auto& hookEngine = scriptEngine.GetHookEngine();
            if (!hookEngine.IsValidHookForPlugin(hookType, *owner))
            {
                JS_ThrowPlainError(ctx, "Hook type not available for this plugin type.");
                return JS_EXCEPTION;
            }

            auto cookie = hookEngine.Subscribe(hookType, owner, callback);
            return gScDisposable.New(ctx, [&hookEngine, hookType, cookie]() { hookEngine.Unsubscribe(hookType, cookie); });
        }

        static JSValue queryAction(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(action, ctx, argv[0]);
            JS_UNPACK_OBJECT(args, ctx, argv[1]);

            return QueryOrExecuteAction(ctx, action, args, JSCallback(ctx, argv[2]), false);
        }

        static JSValue executeAction(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(action, ctx, argv[0]);
            JS_UNPACK_OBJECT(args, ctx, argv[1]);

            return QueryOrExecuteAction(ctx, action, args, JSCallback(ctx, argv[2]), true);
        }

        static JSValue QueryOrExecuteAction(
            JSContext* ctx, const std::string& actionid, JSValue args, const JSCallback& callback, bool isExecute)
        {
            auto& scriptEngine = GetContext()->GetScriptEngine();
            auto plugin = scriptEngine.GetExecInfo().GetCurrentPlugin();
            auto pair = scriptEngine.CreateGameAction(ctx, actionid, args, plugin->GetMetadata().Name);

            std::unique_ptr<GameActions::GameAction> action = std::move(pair.first);

            if (pair.second)
                return JS_ThrowPlainError(ctx, "Invalid action parameters.");

            if (action != nullptr)
            {
                if (isExecute)
                {
                    action->SetCallback(
                        [plugin, callback](const GameActions::GameAction* act, const GameActions::Result* res) -> void {
                            HandleGameActionResult(plugin, *act, *res, callback);
                        });
                    GameActions::Execute(action.get(), getGameState());
                }
                else
                {
                    auto res = GameActions::Query(action.get(), getGameState());
                    HandleGameActionResult(plugin, *action, res, callback);
                }
            }
            else
            {
                return JS_ThrowPlainError(ctx, "Unknown action.");
            }
            return JS_UNDEFINED;
        }

        static void HandleGameActionResult(
            const std::shared_ptr<Plugin>& plugin, const GameActions::GameAction& action, const GameActions::Result& res,
            const JSCallback& callback)
        {
            auto& scriptEngine = GetContext()->GetScriptEngine();
            JSContext* ctx = plugin ? plugin->GetContext() : scriptEngine.GetContext();
            JSValue jsResult = scriptEngine.GameActionResultToJS(ctx, action, res);
            // Call the plugin callback and pass the result object
            scriptEngine.ExecutePluginCall(plugin, callback.callback, { jsResult }, false);
        }

        static JSValue registerAction(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(action, ctx, argv[0]);
            JS_UNPACK_CALLBACK(query, ctx, argv[1]);
            JS_UNPACK_CALLBACK(execute, ctx, argv[2]);

            auto& scriptEngine = GetContext()->GetScriptEngine();
            auto plugin = scriptEngine.GetExecInfo().GetCurrentPlugin();
            if (!scriptEngine.RegisterCustomAction(plugin, action, query, execute))
            {
                JS_ThrowPlainError(ctx, "action has already been registered.");
                return JS_EXCEPTION;
            }

            return JS_UNDEFINED;
        }

        static int32_t SetIntervalOrTimeout(const JSCallback& callback, int32_t delay, bool repeat)
        {
            auto& scriptEngine = GetContext()->GetScriptEngine();
            auto plugin = scriptEngine.GetExecInfo().GetCurrentPlugin();

            return scriptEngine.AddInterval(plugin, delay, repeat, callback);
        }

        static void ClearIntervalOrTimeout(int32_t handle)
        {
            auto& scriptEngine = GetContext()->GetScriptEngine();
            auto plugin = scriptEngine.GetExecInfo().GetCurrentPlugin();
            scriptEngine.RemoveInterval(plugin, handle);
        }

        static JSValue setInterval(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_CALLBACK(callback, ctx, argv[0]);
            JS_UNPACK_INT32(delay, ctx, argv[1]);
            return JS_NewInt32(ctx, SetIntervalOrTimeout(callback, delay, true));
        }

        static JSValue setTimeout(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_CALLBACK(callback, ctx, argv[0]);
            JS_UNPACK_INT32(delay, ctx, argv[1]);
            return JS_NewInt32(ctx, SetIntervalOrTimeout(callback, delay, false));
        }

        static JSValue clearInterval(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_INT32(handle, ctx, argv[0]);
            ClearIntervalOrTimeout(handle);
            return JS_UNDEFINED;
        }

        static JSValue clearTimeout(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_INT32(handle, ctx, argv[0]);
            ClearIntervalOrTimeout(handle);
            return JS_UNDEFINED;
        }

        static JSValue getIcon(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(iconName, ctx, argv[0]);
            return JS_NewInt64(ctx, GetIconByName(iconName));
        }

        static JSValue base64encode(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JSValue data = argv[0];
            size_t size = 0;
            uint8_t* bytes = JS_GetUint8Array(ctx, &size, data);
            if (bytes == nullptr)
            {
                return JS_ThrowTypeError(ctx, "Expected Uint8Array");
            }

            try
            {
                std::string encoded = base64::to_base64(std::string_view(reinterpret_cast<const char*>(bytes), size));
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "data", JSFromStdString(ctx, encoded));
                return obj;
            }
            catch (const std::exception& e)
            {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 1));
                JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, e.what()));
                return obj;
            }
        }

        static JSValue base64decode(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(str, ctx, argv[0]);
            try
            {
                std::vector<uint8_t> decoded = base64::decode_into<std::vector<uint8_t>>(str);
                JSValue data = JS_NewUint8ArrayCopy(ctx, decoded.data(), decoded.size());
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "data", data);
                return obj;
            }
            catch (const std::exception& e)
            {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 1));
                JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, e.what()));
                return obj;
            }
        }

        static JSValue zstdCompress(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JSValue data = argv[0];
            size_t size = 0;
            uint8_t* bytes = JS_GetUint8Array(ctx, &size, data);
            if (bytes == nullptr)
            {
                return JS_ThrowTypeError(ctx, "Expected Uint8Array");
            }

            int32_t level = Compression::kZstdDefaultCompressionLevel;
            if (argc > 1 && JS_IsNumber(argv[1]))
            {
                JS_ToInt32(ctx, &level, argv[1]);
            }

            try
            {
                MemoryStream source(bytes, size);
                MemoryStream dest;
                if (Compression::zstdCompress(source, size, dest, Compression::ZstdMetadata::both, level))
                {
                    JSValue obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(
                        ctx, obj, "data",
                        JS_NewUint8ArrayCopy(ctx, static_cast<const uint8_t*>(dest.GetData()), dest.GetLength()));
                    return obj;
                }
                else
                {
                    JSValue obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 1));
                    JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, "Compression failed"));
                    return obj;
                }
            }
            catch (const std::exception& e)
            {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 1));
                JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, e.what()));
                return obj;
            }
        }

        static JSValue zstdDecompress(JSContext* ctx, JSValue thisVal, int argc, JSValue* argv)
        {
            JSValue data = argv[0];
            size_t size = 0;
            uint8_t* bytes = JS_GetUint8Array(ctx, &size, data);
            if (bytes == nullptr)
            {
                return JS_ThrowTypeError(ctx, "Expected Uint8Array");
            }

            int64_t requestedLength = -1;
            if (argc > 1 && JS_IsNumber(argv[1]))
            {
                JS_ToInt64(ctx, &requestedLength, argv[1]);
            }

            if (requestedLength < -1)
            {
                return JS_ThrowRangeError(ctx, "Length must be >= 0 or -1");
            }

            try
            {
                unsigned long long contentSize = ZSTD_getFrameContentSize(bytes, size);
                if (contentSize == ZSTD_CONTENTSIZE_ERROR)
                {
                    JSValue obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 1));
                    JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, "Not a valid zstd frame"));
                    return obj;
                }

                if (requestedLength == 0)
                {
                    JSValue obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, obj, "data", JS_NewUint8ArrayCopy(ctx, bytes, 0));
                    JS_SetPropertyStr(ctx, obj, "moreAvailable", JS_NewBool(ctx, true));
                    if (contentSize != ZSTD_CONTENTSIZE_UNKNOWN)
                    {
                        JS_SetPropertyStr(ctx, obj, "totalSize", JS_NewInt64(ctx, contentSize));
                    }
                    return obj;
                }

                if (requestedLength == -1 && contentSize != ZSTD_CONTENTSIZE_UNKNOWN)
                {
                    requestedLength = contentSize;
                }

                std::vector<uint8_t> outputBuffer;
                ZSTD_inBuffer input = { bytes, size, 0 };
                const auto dctxDeleter = [](ZSTD_DCtx* ptr) { ZSTD_freeDCtx(ptr); };
                std::unique_ptr<ZSTD_DCtx, decltype(dctxDeleter)> dctx(ZSTD_createDCtx(), dctxDeleter);
                bool moreAvailable = false;

                if (requestedLength != -1)
                {
                    outputBuffer.resize(static_cast<size_t>(requestedLength));
                    ZSTD_outBuffer output = { outputBuffer.data(), outputBuffer.size(), 0 };
                    size_t const ret = ZSTD_decompressStream(dctx.get(), &output, &input);
                    if (ZSTD_isError(ret))
                    {
                        JSValue obj = JS_NewObject(ctx);
                        JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 3));
                        JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, ZSTD_getErrorName(ret)));
                        return obj;
                    }
                    outputBuffer.resize(output.pos);
                    moreAvailable = (ret > 0) || (input.pos < input.size);
                }
                else
                {
                    uint8_t temp[16384];
                    size_t ret;
                    do
                    {
                        ZSTD_outBuffer output = { temp, sizeof(temp), 0 };
                        ret = ZSTD_decompressStream(dctx.get(), &output, &input);
                        if (ZSTD_isError(ret))
                        {
                            JSValue obj = JS_NewObject(ctx);
                            JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 3));
                            JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, ZSTD_getErrorName(ret)));
                            return obj;
                        }
                        outputBuffer.insert(outputBuffer.end(), temp, temp + output.pos);
                    } while (ret > 0);
                    moreAvailable = (input.pos < input.size);
                }

                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "data", JS_NewUint8ArrayCopy(ctx, outputBuffer.data(), outputBuffer.size()));
                JS_SetPropertyStr(ctx, obj, "moreAvailable", JS_NewBool(ctx, moreAvailable));
                if (contentSize != ZSTD_CONTENTSIZE_UNKNOWN)
                {
                    JS_SetPropertyStr(ctx, obj, "totalSize", JS_NewInt64(ctx, contentSize));
                }
                return obj;
            }
            catch (const std::exception& e)
            {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "error", JS_NewInt32(ctx, 4));
                JS_SetPropertyStr(ctx, obj, "message", JSFromStdString(ctx, e.what()));
                return obj;
            }
        }

    public:
        void Register(JSContext* ctx)
        {
            static constexpr JSCFunctionListEntry funcs[] = {
                JS_CGETSET_DEF("apiVersion", ScContext::apiVersion_get, nullptr),
                JS_CGETSET_DEF("configuration", ScContext::configuration_get, nullptr),
                JS_CGETSET_DEF("sharedStorage", ScContext::sharedStorage_get, nullptr),
                JS_CFUNC_DEF("getParkStorage", 1, ScContext::getParkStorage),
                JS_CGETSET_DEF("mode", ScContext::mode_get, nullptr),
                JS_CGETSET_DEF("paused", ScContext::paused_get, &ScContext::paused_set),
                JS_CFUNC_DEF("captureImage", 1, ScContext::captureImage),
                JS_CFUNC_DEF("getObject", 2, ScContext::getObject),
                JS_CFUNC_DEF("getAllObjects", 2, ScContext::getAllObjects),
                JS_CFUNC_DEF("getTrackSegment", 1, ScContext::getTrackSegment),
                JS_CFUNC_DEF("getAllTrackSegments", 0, ScContext::getAllTrackSegments),
                JS_CFUNC_DEF("getRandom", 2, ScContext::getRandom),
                JS_CFUNC_DEF("formatString", 0, ScContext::formatString),
                JS_CFUNC_DEF("subscribe", 2, ScContext::subscribe),
                JS_CFUNC_DEF("queryAction", 3, ScContext::queryAction),
                JS_CFUNC_DEF("executeAction", 3, ScContext::executeAction),
                JS_CFUNC_DEF("registerAction", 3, ScContext::registerAction),
                JS_CFUNC_DEF("setInterval", 2, ScContext::setInterval),
                JS_CFUNC_DEF("setTimeout", 2, ScContext::setTimeout),
                JS_CFUNC_DEF("clearInterval", 1, ScContext::clearInterval),
                JS_CFUNC_DEF("clearTimeout", 1, ScContext::clearTimeout),
                JS_CFUNC_DEF("getIcon", 1, ScContext::getIcon),
                JS_CFUNC_DEF("base64encode", 1, ScContext::base64encode),
                JS_CFUNC_DEF("base64decode", 1, ScContext::base64decode),
                JS_CFUNC_DEF("zstdCompress", 1, ScContext::zstdCompress),
                JS_CFUNC_DEF("zstdDecompress", 1, ScContext::zstdDecompress),
            };
            RegisterBase(ctx, "Context", nullptr, funcs);
        }

        JSValue New(JSContext* ctx)
        {
            return MakeWithOpaque(ctx, nullptr);
        }
    };
} // namespace OpenRCT2::Scripting

#endif
