/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Drawing.h"

#include <cassert>
#include <cstring>

template<DrawBlendOp TBlendOp>
static void FASTCALL DrawRLESpriteMagnify(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    auto& paletteMap = args.PalMap;
    auto imgData = args.SourceImage.offset;
    auto dst = args.DestinationBits;
    auto srcX = args.SrcX;
    auto srcY = args.SrcY;
    auto width = args.Width;
    auto height = args.Height;
    auto zoom = dpi.zoom_level;
    auto dstLineWidth = dpi.LineStride();

    for (int32_t y = 0; y < height; y++)
    {
        uint8_t* nextDst = dst + dstLineWidth;
        const int32_t rowNum = zoom.ApplyTo(srcY + y);
        uint16_t lineOffset;
        std::memcpy(&lineOffset, &imgData[rowNum * sizeof(uint16_t)], sizeof(uint16_t));
        const uint8_t* data8 = imgData + lineOffset;

        bool lastDataForLine = false;
        int32_t numPixels = 0;
        uint8_t pixelRunStart = 0;
        for (int32_t x = 0; x < width; x++)
        {
            const int32_t colNum = zoom.ApplyTo(srcX + x);

            while (colNum >= pixelRunStart + numPixels && !lastDataForLine)
            {
                data8 += numPixels;
                numPixels = *data8++;
                pixelRunStart = *data8++;
                lastDataForLine = numPixels & 0x80;
                numPixels &= 0x7F;
            }
            if (pixelRunStart <= colNum && colNum < pixelRunStart + numPixels)
                BlitPixel<TBlendOp>(data8 + colNum - pixelRunStart, dst, paletteMap);
            dst++;
        }

        dst = nextDst;
    }
}

template<DrawBlendOp TBlendOp, size_t TZoom>
static void FASTCALL DrawRLESpriteMinify(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    auto src0 = args.SourceImage.offset;
    auto dst0 = args.DestinationBits;
    auto srcX = args.SrcX;
    auto srcY = args.SrcY;
    auto width = args.Width;
    auto height = args.Height;
    auto zoom = 1 << TZoom;
    auto dstLineWidth = static_cast<size_t>(dpi.LineStride());

    // Move up to the first line of the image if source_y_start is negative. Why does this even occur?
    if (srcY < 0)
    {
        srcY += zoom;
        height -= zoom;
        dst0 += dstLineWidth;
    }

    // For every line in the image
    for (int32_t i = 0; i < height; i += zoom)
    {
        int32_t y = srcY + i;

        // The first part of the source pointer is a list of offsets to different lines
        // This will move the pointer to the correct source line.
        uint16_t lineOffset = src0[y * 2] | (src0[y * 2 + 1] << 8);
        auto nextRun = src0 + lineOffset;
        auto dstLineStart = dst0 + dstLineWidth * (i >> TZoom);

        // For every data chunk in the line
        auto isEndOfLine = false;
        while (!isEndOfLine)
        {
            // Read chunk metadata
            auto src = nextRun;
            auto dataSize = *src++;
            auto firstPixelX = *src++;
            isEndOfLine = (dataSize & 0x80) != 0;
            dataSize &= 0x7F;

            // Have our next source pointer point to the next data section
            nextRun = src + dataSize;

            int32_t x = firstPixelX - srcX;
            int32_t numPixels = dataSize;
            if (x > 0)
            {
                // If x is not a multiple of zoom, round it up to a multiple
                auto mod = x & (zoom - 1);
                if (mod != 0)
                {
                    auto offset = zoom - mod;
                    x += offset;
                    src += offset;
                    numPixels -= offset;
                }
            }
            else if (x < 0)
            {
                // Clamp x to zero if negative
                src += -x;
                numPixels += x;
                x = 0;
            }

            // If the end position is further out than the whole image
            // end position then we need to shorten the line again
            numPixels = std::min(numPixels, width - x);

            auto dst = dstLineStart + (x >> TZoom);
            if constexpr ((TBlendOp & BLEND_SRC) == 0 && (TBlendOp & BLEND_DST) == 0 && TZoom == 0)
            {
                // Since we're sampling each pixel at this zoom level, just do a straight std::memcpy
                if (numPixels > 0)
                {
                    std::memcpy(dst, src, numPixels);
                }
            }
            else
            {
                auto& paletteMap = args.PalMap;
                while (numPixels > 0)
                {
                    BlitPixel<TBlendOp>(src, dst, paletteMap);
                    numPixels -= zoom;
                    src += zoom;
                    dst++;
                }
            }
        }
    }
}

template<DrawBlendOp TBlendOp>
static void FASTCALL DrawRLESprite(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    auto zoom_level = static_cast<int8_t>(dpi.zoom_level);
    switch (zoom_level)
    {
        case -2:
        case -1:
            DrawRLESpriteMagnify<TBlendOp>(dpi, args);
            break;
        case 0:
            DrawRLESpriteMinify<TBlendOp, 0>(dpi, args);
            break;
        case 1:
            DrawRLESpriteMinify<TBlendOp, 1>(dpi, args);
            break;
        case 2:
            DrawRLESpriteMinify<TBlendOp, 2>(dpi, args);
            break;
        case 3:
            DrawRLESpriteMinify<TBlendOp, 3>(dpi, args);
            break;
        default:
            assert(false);
            break;
    }
}

/**
 * Transfers readied images onto buffers
 * This function copies the sprite data onto the screen
 *  rct2: 0x0067AA18
 * @param imageId Only flags are used.
 */
void FASTCALL GfxRleSpriteToBuffer(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    if (args.Image.HasPrimary())
    {
        if (args.Image.IsBlended())
        {
            DrawRLESprite<BLEND_TRANSPARENT | BLEND_SRC | BLEND_DST>(dpi, args);
        }
        else
        {
            DrawRLESprite<BLEND_TRANSPARENT | BLEND_SRC>(dpi, args);
        }
    }
    else if (args.Image.IsBlended())
    {
        DrawRLESprite<BLEND_TRANSPARENT | BLEND_DST>(dpi, args);
    }
    else
    {
        DrawRLESprite<BLEND_TRANSPARENT>(dpi, args);
    }
}

#include <cstddef>
#include <cstdint>

static uint8_t palette[256] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};
static PaletteMap pm(palette);


bool PaletteMap::operator==(const PaletteMap& lhs) const
{
    return _data == lhs._data && _dataLength == lhs._dataLength && _numMaps == lhs._numMaps && _mapLength == lhs._mapLength;
}

uint8_t& PaletteMap::operator[](size_t index)
{
    assert(index < _dataLength);

    // Provide safety in release builds
    if (index >= _dataLength)
    {
        static uint8_t dummy;
        return dummy;
    }

    return _data[index];
}

uint8_t PaletteMap::operator[](size_t index) const
{
    assert(index < _dataLength);

    // Provide safety in release builds
    if (index >= _dataLength)
    {
        return 0;
    }

    return _data[index];
}

uint8_t PaletteMap::Blend(uint8_t src, uint8_t dst) const
{
    // src = 0 would be transparent so there is no blend palette for that, hence (src - 1)
    assert(src != 0 && (src - 1) < _numMaps);
    assert(dst < _mapLength);
    auto idx = ((src - 1) * 256) + dst;
    return (*this)[idx];
}

ZoomLevel& ZoomLevel::operator=(const ZoomLevel& other)
{
    _level = other._level;
    return *this;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < sizeof(DrawPixelInfo) + sizeof(G1Element) + 16)
        return 0;

    // Create DrawPixelInfo from first part of fuzzer data
    DrawPixelInfo dpi;
    memcpy(&dpi, data, sizeof(DrawPixelInfo));
    
    // Ensure reasonable bounds for allocated buffers
    dpi.width = dpi.width & 0x7FF;  // Max 2047
    dpi.height = dpi.height & 0x7FF;
    dpi.zoom_level = static_cast<ZoomLevel>(static_cast<int8_t>(dpi.zoom_level) & 0x3); // Valid zoom levels 0-3
    
    // Allocate buffer for destination bits
    std::vector<uint8_t> bits(dpi.width * dpi.height);
    dpi.bits = bits.data();

    G1Element g1Element;
    memcpy(&g1Element, data + sizeof(DrawPixelInfo), sizeof(G1Element));
    g1Element.offset = const_cast<uint8_t*>(data) + sizeof(DrawPixelInfo) + sizeof(G1Element);


    // Create DrawSpriteArgs from remaining fuzzer data
    const uint8_t* remaining = data + sizeof(DrawPixelInfo) + sizeof(G1Element);
    
    int32_t args_srcX;
    int32_t args_srcY;
    int32_t args_width;
    int32_t args_height;
    uint8_t* args_srcImage;
    memcpy(&args_srcX, remaining, 4);
    memcpy(&args_srcY, remaining + 4, 4);
    memcpy(&args_width, remaining + 8, 4);
    memcpy(&args_height, remaining + 12, 4);
    args_width &= 0x7FF;
    args_height &= 0x7FF;
    args_srcImage = const_cast<uint8_t*>(remaining) + 16;

    DrawSpriteArgs args(ImageId(), pm, g1Element, args_srcX, args_srcY, args_width, args_height, dpi.bits);

    // Call target function
    GfxRleSpriteToBuffer(dpi, args);

    return 0;
}
