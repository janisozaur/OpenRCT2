/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "G1Element.h"
#include <cstring>
#include <cstdint>

namespace OpenRCT2::Drawing
{
    template<bool TCheckBounds, typename TRowRunVisitor>
    bool IterateRleSprite(const G1Element& g1, const uint8_t* end, TRowRunVisitor&& visitor)
    {
        const uint8_t* data = g1.offset;
        if (data == nullptr || g1.height <= 0)
            return false;

        if constexpr (TCheckBounds)
        {
            if (end != nullptr && data + g1.height * sizeof(uint16_t) > end)
                return false;
        }

        for (int32_t y = 0; y < g1.height; y++)
        {
            uint16_t lineOffset;
            std::memcpy(&lineOffset, data + y * sizeof(uint16_t), sizeof(uint16_t));
            const uint8_t* ptr = data + lineOffset;

            if constexpr (TCheckBounds)
            {
                if (end != nullptr && (ptr < data || ptr >= end))
                    return false;
            }

            bool isEndOfLine = false;
            while (!isEndOfLine)
            {
                if constexpr (TCheckBounds)
                {
                    if (end != nullptr && ptr + 2 > end)
                        return false;
                }

                uint8_t chunk0 = *ptr++;
                uint8_t firstPixelX = *ptr++;
                uint8_t numPixels = chunk0 & 0x7F;
                isEndOfLine = (chunk0 & 0x80) != 0;

                if constexpr (TCheckBounds)
                {
                    if (static_cast<int32_t>(firstPixelX) + numPixels > g1.width)
                        return false;
                    if (end != nullptr && ptr + numPixels > end)
                        return false;
                }

                if (!visitor(y, firstPixelX, numPixels, ptr))
                    return false;

                ptr += numPixels;
            }
        }
        return true;
    }
} // namespace OpenRCT2::Drawing
