/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/
#pragma once

#include "../core/Tracing.h"

#define PROFILING_STR(x) #x
#define PROFILING_STRINGIFY(x) PROFILING_STR(x)

namespace OpenRCT2::Profiling
{
#if defined(__clang__) || defined(__GNUC__)
    #define PROFILING_FUNC_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define PROFILING_FUNC_NAME __FUNCSIG__
#else
    #error "Unsupported compiler"
#endif

#if defined(__clang_major__) && __clang_major__ <= 5
    // Clang 5 crashes using the profiler, we need to disable it.
    #define PROFILED_FUNCTION()
#else
    #ifndef PROFILING_CATEGORY
        #define PROFILING_CATEGORY game
    #endif

    #define PROFILED_FUNCTION() TRACE_EVENT(PROFILING_STRINGIFY(PROFILING_CATEGORY), PROFILING_FUNC_NAME)
#endif

} // namespace OpenRCT2::Profiling
