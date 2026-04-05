/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifdef USE_LIBUV
    #include <uv.h>

namespace OpenRCT2::Platform
{
    class LibuvLoop
    {
    private:
        uv_loop_t* _loop;

    public:
        LibuvLoop();
        ~LibuvLoop();

        static LibuvLoop& Get();

        uv_loop_t* GetLoop() const
        {
            return _loop;
        }

        void Tick();
    };
} // namespace OpenRCT2::Platform
#endif
