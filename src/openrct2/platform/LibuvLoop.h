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

    #include <functional>
    #include <string>
    #include <uv.h>
    #include <vector>

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
        void Run(uv_run_mode mode);
    };

    /**
     * Reads all bytes from a file asynchronously.
     * @param path The path to the file.
     * @param callback The callback to call when the file is read. The first parameter is the data, the second is the error code
     * (0 for success).
     */
    void ReadAllBytesAsync(const std::string& path, std::function<void(std::vector<uint8_t>, int)> callback);
} // namespace OpenRCT2::Platform

#endif
