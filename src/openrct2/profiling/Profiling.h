/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/
#pragma once

#include "ProfilingMacros.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace OpenRCT2::Profiling
{
    void enable();
    void disable();
    bool isEnabled();

    struct Function
    {
        virtual ~Function() = default;

        virtual const char* getName() const noexcept = 0;
        virtual uint64_t getCallCount() const noexcept = 0;
        virtual std::vector<double> getTimeSamples() const = 0;
        virtual double getTotalTime() const = 0;
        virtual double getMinTime() const = 0;
        virtual double getMaxTime() const = 0;

        double getAverageTime() const
        {
            return 0.0;
        }

        virtual std::vector<Function*> getParents() const = 0;
        virtual std::vector<Function*> getChildren() const = 0;
    };

    void resetData();
    const std::vector<Function*>& getData();
    [[nodiscard]] bool exportData(const std::string& filePath);

} // namespace OpenRCT2::Profiling
