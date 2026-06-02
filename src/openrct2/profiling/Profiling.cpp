/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Profiling.h"

#include <atomic>

namespace OpenRCT2::Profiling
{
    // Global enable flag, atomic for thread safety
    static std::atomic<bool> _enabled{ false };

    void enable()
    {
        _enabled.store(true, std::memory_order_release);
    }

    void disable()
    {
        _enabled.store(false, std::memory_order_release);
    }

    bool isEnabled()
    {
        return _enabled.load(std::memory_order_acquire);
    }

    const std::vector<Function*>& getData()
    {
        static std::vector<Function*> empty;
        return empty;
    }

    void resetData()
    {
    }

    bool exportData(const std::string& filePath)
    {
        return false;
    }

} // namespace OpenRCT2::Profiling
