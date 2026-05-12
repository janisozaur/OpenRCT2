/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../core/Guard.hpp"
#include "Drawing.Sprite.h"
#include "Drawing.h"
#include "PaletteIndex.h"

#include <algorithm>

using OpenRCT2::Drawing::PaletteIndex;

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512DQ__) && defined(__AVX512VL__) && defined(__AVX512VBMI__)

    #include <immintrin.h>

void FilterRectAvx512(PaletteIndex* dst, int32_t width, int32_t height, int32_t stride, const PaletteIndex* paletteMap)
{
    const __m512i map = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(paletteMap));
    const __m512i map2 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(paletteMap + 64));
    const __m512i map3 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(paletteMap + 128));
    const __m512i map4 = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(paletteMap + 192));

    for (int32_t yy = 0; yy < height; yy++)
    {
        PaletteIndex* d = dst + yy * stride;
        int32_t ww = width;
        for (; ww >= 64; ww -= 64)
        {
            __m512i indices = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(d));

            // VBMI _mm512_permutex2var_epi8 takes two tables and indices.
            // Our palette map is 256 bytes, so we need two calls to cover all 256 indices.
            // But wait, permute2var handles 128 indices (2*64).
            // For 256 we need more.
            // map1: 0-63, map2: 64-127, map3: 128-191, map4: 192-255

            __m512i res12 = _mm512_permutex2var_epi8(map, indices, map2);
            __m512i res34 = _mm512_permutex2var_epi8(map3, indices, map4);

            // Select between res12 and res34 based on the 7th bit of indices
            __mmask64 highBitMask = _mm512_test_epi8_mask(indices, _mm512_set1_epi8(static_cast<int8_t>(0x80)));
            __m512i res = _mm512_mask_blend_epi8(highBitMask, res12, res34);

            _mm512_storeu_si512(reinterpret_cast<__m512i*>(d), res);
            d += 64;
        }

        if (ww > 0)
        {
            const __mmask64 tailMask = _cvtu64_mask64((1ULL << ww) - 1ULL);
            __m512i indices = _mm512_maskz_loadu_epi8(tailMask, d);

            __m512i res12 = _mm512_permutex2var_epi8(map, indices, map2);
            __m512i res34 = _mm512_permutex2var_epi8(map3, indices, map4);

            __mmask64 highBitMask = _mm512_test_epi8_mask(indices, _mm512_set1_epi8(static_cast<int8_t>(0x80)));
            __m512i res = _mm512_mask_blend_epi8(highBitMask, res12, res34);

            _mm512_mask_storeu_epi8(d, tailMask, res);
        }
    }
}

void LightFxRenderToTextureAvx512(
    void* dstPixels, uint32_t dstPitch, const PaletteIndex* bits, uint32_t width, uint32_t height, const uint32_t* palette,
    const uint32_t* lightPalette, const uint8_t* lightBits)
{
    const __m512i v6 = _mm512_set1_epi32(6);
    const __m512i v255 = _mm512_set1_epi32(255);
    const __m512i maskFF = _mm512_set1_epi32(0xFF);
    const __m512i zero = _mm512_setzero_si512();

    for (uint32_t yy = 0; yy < height; yy++)
    {
        uint32_t* dst = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(dstPixels) + yy * dstPitch);
        const PaletteIndex* b = bits + yy * width;
        const uint8_t* lb = lightBits + yy * width;

        int32_t ww = width;
        while (ww > 0)
        {
            int32_t count = std::min(ww, 16);
            __mmask16 loadMask = static_cast<__mmask16>((1ULL << count) - 1ULL);

            __m128i indices8 = _mm_maskz_loadu_epi8(loadMask, b);
            __m512i indices32 = _mm512_cvtepu8_epi32(indices8);

            __m512i darkColours = _mm512_mask_i32gather_epi32(zero, loadMask, indices32, palette, 4);
            __m512i lightColours = _mm512_mask_i32gather_epi32(zero, loadMask, indices32, lightPalette, 4);

            __m128i intensity8 = _mm_maskz_loadu_epi8(loadMask, lb);
            __m512i intensity32 = _mm512_cvtepu8_epi32(intensity8);

            __mmask16 notZero = _mm512_mask_cmpneq_epi32_mask(loadMask, intensity32, zero);

            if (notZero == 0)
            {
                _mm512_mask_storeu_epi32(dst, loadMask, darkColours);
            }
            else
            {
                __m512i intensityMul = _mm512_mullo_epi32(intensity32, v6);

                auto mixChannel = [&](int shift) {
                    __m512i a = _mm512_and_si512(_mm512_srli_epi32(darkColours, shift), maskFF);
                    __m512i bc = _mm512_and_si512(_mm512_srli_epi32(lightColours, shift), maskFF);
                    __m512i bMul = _mm512_srli_epi32(_mm512_mullo_epi32(bc, intensityMul), 8);
                    return _mm512_min_epu32(_mm512_add_epi32(a, bMul), v255);
                };

                __m512i r0 = mixChannel(0);
                __m512i r1 = mixChannel(8);
                __m512i r2 = mixChannel(16);
                __m512i r3 = mixChannel(24);

                __m512i res = _mm512_or_si512(
                    _mm512_or_si512(r0, _mm512_slli_epi32(r1, 8)),
                    _mm512_or_si512(_mm512_slli_epi32(r2, 16), _mm512_slli_epi32(r3, 24)));

                _mm512_mask_storeu_epi32(dst, loadMask, res);
            }

            b += count;
            lb += count;
            dst += count;
            ww -= count;
        }
    }
}

void MaskAvx512(
    int32_t width, int32_t height, const uint8_t* RESTRICT maskSrc, const uint8_t* RESTRICT colourSrc,
    PaletteIndex* RESTRICT dst, int32_t maskWrap, int32_t colourWrap, int32_t dstWrap)
{
    if (width >= 64)
    {
        const __m512i zero = _mm512_setzero_si512();
        for (int32_t yy = 0; yy < height; yy++)
        {
            uint8_t const* m = maskSrc + yy * (maskWrap + width);
            uint8_t const* c = colourSrc + yy * (colourWrap + width);
            PaletteIndex* d = dst + yy * (dstWrap + width);

            int32_t ww = width;
            for (; ww >= 64; ww -= 64)
            {
                const __m512i colour = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(c));
                const __m512i mask = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(m));
                const __m512i dest = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(d));

                const __m512i mc = _mm512_and_si512(colour, mask);
                const __mmask64 notZeroMask = _mm512_cmpneq_epi8_mask(mc, zero);
                const __m512i blended = _mm512_mask_blend_epi8(notZeroMask, dest, mc);

                _mm512_storeu_si512(reinterpret_cast<__m512i*>(d), blended);
                m += 64;
                c += 64;
                d += 64;
            }

            if (ww > 0)
            {
                const __mmask64 tailMask = _cvtu64_mask64((1ULL << ww) - 1ULL);
                const __m512i colour = _mm512_maskz_loadu_epi8(tailMask, c);
                const __m512i mask = _mm512_maskz_loadu_epi8(tailMask, m);
                const __m512i dest = _mm512_maskz_loadu_epi8(tailMask, d);

                const __m512i mc = _mm512_and_si512(colour, mask);
                const __mmask64 notZeroMask = _mm512_cmpneq_epi8_mask(mc, zero);
                const __m512i blended = _mm512_mask_blend_epi8(notZeroMask, dest, mc);

                _mm512_mask_storeu_epi8(d, tailMask, blended);
            }
        }
    }
    else if (width == 32)
    {
        const int32_t maskWrapSIMD = maskWrap + 32;
        const int32_t colourWrapSIMD = colourWrap + 32;
        const int32_t dstWrapSIMD = dstWrap + 32;
        const __m256i zero = _mm256_setzero_si256();
        for (int32_t yy = 0; yy < height; yy++)
        {
            const __m256i colour = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(colourSrc + yy * colourWrapSIMD));
            const __m256i mask = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(maskSrc + yy * maskWrapSIMD));
            const __m256i dest = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + yy * dstWrapSIMD));

            const __m256i mc = _mm256_and_si256(colour, mask);

            const __mmask32 notZeroMask = _mm256_cmpneq_epi8_mask(mc, zero);

            const __m256i blended = _mm256_mask_blend_epi8(notZeroMask, dest, mc);

            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + yy * dstWrapSIMD), blended);
        }
    }
    else
    {
        MaskScalar(width, height, maskSrc, colourSrc, dst, maskWrap, colourWrap, dstWrap);
    }
}

#else

    #ifdef OPENRCT2_X86
        #error You have to compile this file with AVX-512 enabled, when targeting x86!
    #endif

void FilterRectAvx512(PaletteIndex* RESTRICT dst, int32_t width, int32_t height, int32_t stride, const PaletteIndex* paletteMap)
{
    OpenRCT2::Guard::Fail("AVX-512 FilterRect function called on a CPU that doesn't support AVX-512");
}

void LightFxRenderToTextureAvx512(
    void* dstPixels, uint32_t dstPitch, const PaletteIndex* bits, uint32_t width, uint32_t height, const uint32_t* palette,
    const uint32_t* lightPalette, const uint8_t* lightBits)
{
    OpenRCT2::Guard::Fail("AVX-512 LightFX function called on a CPU that doesn't support AVX-512");
}

void MaskAvx512(
    int32_t width, int32_t height, const uint8_t* RESTRICT maskSrc, const uint8_t* RESTRICT colourSrc,
    PaletteIndex* RESTRICT dst, int32_t maskWrap, int32_t colourWrap, int32_t dstWrap)
{
    OpenRCT2::Guard::Fail("AVX-512 Mask function called on a CPU that doesn't support AVX-512");
}

#endif
