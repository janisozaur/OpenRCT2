/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef USE_LIBUV
    #include "LibuvLoop.h"

    #include "../Diagnostic.h"

    #include <uv.h>

namespace OpenRCT2::Platform
{
    LibuvLoop::LibuvLoop()
    {
        _loop = uv_default_loop();
        if (_loop == nullptr)
        {
            LOG_FATAL("Unable to initialize libuv default loop.");
        }
    }

    LibuvLoop::~LibuvLoop()
    {
        // uv_loop_close(_loop); // The default loop should not be closed manually until exit
    }

    LibuvLoop& LibuvLoop::Get()
    {
        static LibuvLoop instance;
        return instance;
    }

    void LibuvLoop::Tick()
    {
        uv_run(_loop, UV_RUN_NOWAIT);
    }
} // namespace OpenRCT2::Platform
#endif
