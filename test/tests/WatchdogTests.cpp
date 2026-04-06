/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/core/Watchdog.hpp>
#include <openrct2/platform/Platform.h>

#ifdef ENABLE_WATCHDOG

using namespace OpenRCT2;

TEST(WatchdogTests, heartbeat_prevents_hang)
{
    Watchdog watchdog;
    watchdog.SetTimeout(200);
    watchdog.Start();

    for (int i = 0; i < 5; i++)
    {
        Platform::Sleep(100);
        watchdog.Heartbeat();
    }

    watchdog.Stop();
    // If we reach here without triggering (which would hang or crash in real use,
    // but here we just test it doesn't trigger unexpectedly)
}

TEST(WatchdogTests, pause_prevents_hang)
{
    Watchdog watchdog;
    watchdog.SetTimeout(100);
    watchdog.Start();
    watchdog.Pause();

    Platform::Sleep(300);

    watchdog.Resume();
    watchdog.Stop();
}

#endif
