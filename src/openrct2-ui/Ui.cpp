/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Ui.h"

#include "SDLException.h"
#include "UiContext.h"
#include "audio/AudioContext.h"
#include "drawing/BitmapReader.h"

#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Diagnostic.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/StartupLogger.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/command_line/CommandLine.hpp>
#include <openrct2/platform/Platform.h>
#include <openrct2/ui/UiContext.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

using namespace OpenRCT2;
using namespace OpenRCT2::Audio;
using namespace OpenRCT2::Ui;

template<typename T>
static std::shared_ptr<T> ToShared(std::unique_ptr<T>&& src)
{
    return std::shared_ptr<T>(std::move(src));
}

/**
 * Main entry point for non-Windows systems. Windows instead uses its own DLL proxy.
 */
#if defined(_MSC_VER) && !defined(__DISABLE_DLL_PROXY__)
int NormalisedMain(int argc, const char** argv)
#else
int main(int argc, const char** argv)
#endif
{
#ifdef __EMSCRIPTEN__
    MAIN_THREAD_EM_ASM({
        specialHTMLTargets["!canvas"] = Module.canvas;
        Module.canvas.addEventListener("contextmenu", function(e) { e.preventDefault(); });
    });
#endif
    // Initialize startup logging as early as possible
    StartupLogger::GetInstance().Initialize();
    LOG_STARTUP("OpenRCT2 main() function started");
    LOG_STARTUP("Processing command line arguments...");

    std::unique_ptr<IContext> context;
    int32_t rc = EXIT_SUCCESS;
    int runGame = CommandLineRun(argv, argc);
    LOG_STARTUP("Command line processing completed with result: %d", runGame);

    LOG_STARTUP("Registering bitmap readers...");
    RegisterBitmapReader();
    LOG_STARTUP("Bitmap readers registered");
    if (runGame == EXITCODE_CONTINUE)
    {
        LOG_STARTUP("Starting OpenRCT2 initialization");
        if (gOpenRCT2Headless)
        {
            // Run OpenRCT2 with a plain context
            LOG_STARTUP("Running in headless mode");
            context = CreateContext();
            LOG_STARTUP("Headless context created successfully");
        }
        else
        {
            // Run OpenRCT2 with a UI context
            LOG_STARTUP("Running in UI mode");
            LOG_STARTUP("Creating platform environment...");
            auto env = CreatePlatformEnvironment();
            LOG_STARTUP("Platform environment created successfully");

            std::unique_ptr<IAudioContext> audioContext;
            try
            {
                LOG_STARTUP("Creating audio context...");
                audioContext = CreateAudioContext();
                LOG_STARTUP("Audio context created successfully");
            }
            catch (const SDLException& e)
            {
                LOG_STARTUP_WARNING("Failed to create audio context: %s", e.what());
                LOG_WARNING("Failed to create audio context. Using dummy audio context. Error message was: %s", e.what());
                audioContext = CreateDummyAudioContext();
                LOG_STARTUP("Dummy audio context created as fallback");
            }

            LOG_STARTUP("Creating UI context...");
            auto uiContext = CreateUiContext(*env);
            LOG_STARTUP("UI context created successfully");

            LOG_STARTUP("Creating main context...");
            context = CreateContext(std::move(env), std::move(audioContext), std::move(uiContext));
            LOG_STARTUP("Main context created successfully");
        }
        LOG_STARTUP("Starting main game loop...");
        rc = context->RunOpenRCT2(argc, argv);
        LOG_STARTUP("Main game loop completed with exit code: %d", rc);
    }
    else if (runGame == EXITCODE_FAIL)
    {
        LOG_STARTUP("Command line processing failed");
        rc = EXIT_FAILURE;
    }
    else
    {
        LOG_STARTUP("Application exited without starting main loop (exit code: %d)", runGame);
    }

    // Close the startup logger before exiting
    StartupLogger::GetInstance().Close();
    return rc;
}

#ifdef __ANDROID__
extern "C" {
int SDL_main(int argc, const char* argv[])
{
    return main(argc, argv);
}
}
#endif
