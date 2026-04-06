/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef ENABLE_WATCHDOG

    #include "Watchdog.hpp"

    #include "../Context.h"
    #include "../OpenRCT2.h"
    #include "../Version.h"
    #include "../platform/Platform.h"

    #ifdef _WIN32
        #ifndef WIN32_LEAN_AND_MEAN
            #define WIN32_LEAN_AND_MEAN
        #endif
        #include <windows.h>
    #else
        #ifdef __APPLE__
            #include <SDL.h>
        #else
            #include <SDL2/SDL.h>
        #endif
    #endif

namespace OpenRCT2
{
    static Watchdog _watchdog;

    Watchdog& GetWatchdog()
    {
        return _watchdog;
    }

    Watchdog::Watchdog()
    {
    }

    Watchdog::~Watchdog()
    {
        Stop();
    }

    void Watchdog::Start()
    {
        if (_running)
            return;

        _running = true;
        _lastHeartbeat = Platform::GetTicks();
        _watchdogThread = std::thread(&Watchdog::WatchdogLoop, this);
    }

    void Watchdog::Stop()
    {
        if (!_running)
            return;

        _running = false;
        if (_watchdogThread.joinable())
        {
            _watchdogThread.join();
        }
    }

    void Watchdog::Heartbeat()
    {
        _lastHeartbeat = Platform::GetTicks();
    }

    void Watchdog::SetCulprit(const std::string& type, const std::string& name)
    {
        std::lock_guard<std::mutex> lock(_culpritMutex);
        _culpritType = type;
        _culpritName = name;
        _culpritNameCallback = nullptr;
    }

    void Watchdog::SetCulprit(const std::string& type, std::function<std::string()> nameCallback)
    {
        std::lock_guard<std::mutex> lock(_culpritMutex);
        _culpritType = type;
        _culpritName = "";
        _culpritNameCallback = std::move(nameCallback);
    }

    void Watchdog::Pause()
    {
        _paused = true;
    }

    void Watchdog::Resume()
    {
        _paused = false;
        Heartbeat();
    }

    void Watchdog::SetTimeout(uint32_t timeoutMs)
    {
        _timeoutMs = timeoutMs;
    }

    void Watchdog::WatchdogLoop()
    {
        while (_running)
        {
            Platform::Sleep(100);

            if (_paused)
            {
                continue;
            }

            uint32_t now = Platform::GetTicks();
            uint32_t last = _lastHeartbeat.load();

            if (now - last > _timeoutMs)
            {
                TriggerHang();
                // After triggering, we reset the heartbeat to avoid spamming the message
                Heartbeat();
            }
        }
    }

    void Watchdog::TriggerHang()
    {
        std::string type;
        std::string name;
        {
            std::lock_guard<std::mutex> lock(_culpritMutex);
            type = _culpritType;
            name = _culpritName;
            if (name.empty() && _culpritNameCallback)
            {
                // We should be careful calling a callback from the watchdog thread if it might access
                // main-thread only data, but here we assume the callback is safe or the hang makes it
                // "safe enough" to try for diagnostics.
                try
                {
                    name = _culpritNameCallback();
                }
                catch (...)
                {
                    name = "Error retrieving name";
                }
            }
        }

        std::string message = "The OpenRCT2 Watchdog has detected that the game has stopped responding for more than "
            + std::to_string(_timeoutMs) + "ms.\n\n";

        if (!type.empty() && !name.empty())
        {
            message += "Likely culprit: " + type + " (" + name + ")\n\n";
        }
        else
        {
            message += "The specific cause of the hang could not be identified.\n\n";
        }

        message += "This is typically caused by a misbehaving plugin or an exceptionally long game action. "
                   "You can change the timeout or disable this feature in your config.ini (watchdog_timeout_ms).\n\n";

    #ifdef USE_BREAKPAD
        message += "Would you like to terminate the game and upload a crash dump for analysis?";

        #ifdef _WIN32
        int result = MessageBoxA(
            nullptr, message.c_str(), OPENRCT2_NAME " Watchdog", MB_YESNO | MB_ICONERROR | MB_SYSTEMMODAL);
        if (result == IDYES)
        {
            // Trigger breakpad crash
            volatile int* p = nullptr;
            (void)*p;
        }
        #else
        const SDL_MessageBoxData messageboxdata = {
            SDL_MESSAGEBOX_ERROR,
            nullptr,
            OPENRCT2_NAME " Watchdog",
            message.c_str(),
            2,
            (const SDL_MessageBoxButtonData[]){
                { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes (Kill & Dump)" },
                { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No (Wait)" },
            },
            nullptr
        };
        int buttonid;
        if (SDL_ShowMessageBox(&messageboxdata, &buttonid) == 0 && buttonid == 1)
        {
            volatile int* p = nullptr;
            (void)*p;
        }
        #endif
    #else
        message += "The game will continue to wait.";
        #ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), OPENRCT2_NAME " Watchdog", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
        #else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, OPENRCT2_NAME " Watchdog", message.c_str(), nullptr);
        #endif
    #endif
    }
} // namespace OpenRCT2

#endif // ENABLE_WATCHDOG
