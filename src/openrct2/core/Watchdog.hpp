/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifdef ENABLE_WATCHDOG

    #include <atomic>
    #include <functional>
    #include <mutex>
    #include <string>
    #include <thread>

namespace OpenRCT2
{
    class Watchdog
    {
    private:
        std::thread _watchdogThread;
        std::atomic<bool> _running{ false };
        std::atomic<uint32_t> _lastHeartbeat{ 0 };
        std::atomic<uint32_t> _timeoutMs{ 10000 };
        std::atomic<bool> _paused{ false };

        std::mutex _culpritMutex;
        std::string _culpritType;
        std::string _culpritName;
        std::function<std::string()> _culpritNameCallback;

    public:
        Watchdog();
        ~Watchdog();

        void Start();
        void Stop();

        /**
         * Notifies the watchdog that the main thread is still alive.
         */
        void Heartbeat();

        /**
         * Sets the current action/object being processed.
         */
        void SetCulprit(const std::string& type, const std::string& name);

        /**
         * Sets the current action/object being processed with a lazy-evaluated name.
         */
        void SetCulprit(const std::string& type, std::function<std::string()> nameCallback);

        /**
         * Pauses the watchdog (e.g. during long-running system operations).
         */
        void Pause();

        /**
         * Resumes the watchdog.
         */
        void Resume();

        /**
         * Sets the timeout in milliseconds.
         */
        void SetTimeout(uint32_t timeoutMs);

    private:
        void WatchdogLoop();
        void TriggerHang();
    };

    Watchdog& GetWatchdog();

    /**
     * RAII scope for setting the current culprit.
     */
    class WatchdogScope
    {
    public:
        WatchdogScope(const std::string& type, const std::string& name)
        {
            GetWatchdog().SetCulprit(type, name);
        }

        WatchdogScope(const std::string& type, std::function<std::string()> nameCallback)
        {
            GetWatchdog().SetCulprit(type, std::move(nameCallback));
        }

        ~WatchdogScope()
        {
            GetWatchdog().SetCulprit("", "");
        }
    };

    /**
     * RAII scope for pausing the watchdog.
     */
    class WatchdogPauseScope
    {
    public:
        WatchdogPauseScope()
        {
            GetWatchdog().Pause();
        }

        ~WatchdogPauseScope()
        {
            GetWatchdog().Resume();
        }
    };
} // namespace OpenRCT2

#endif // ENABLE_WATCHDOG
