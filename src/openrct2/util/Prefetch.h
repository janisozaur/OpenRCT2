/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#if defined(__amd64__) || defined(_M_AMD64) || defined(__i386__) || defined(_M_IX86)
// Don't bother checking for CPUID, prefetch is available since Pentium 4
#    include <xmmintrin.h>
#    define PREFETCH(x) _mm_prefetch(reinterpret_cast<const char*>(x), _MM_HINT_T0)

#elif defined(_MSC_VER) && defined(_M_ARM64)
// PRFM is available since ARMv8.0
#    include <arm64_neon.h>
#    define PREFETCH(x) __prefetch2((x))

#elif defined(__GNUC__)
// Let the compiler handle prefetch instruction
#    define PREFETCH(x) __builtin_prefetch(x)

#else
#    define PREFETCH(x)
#endif
