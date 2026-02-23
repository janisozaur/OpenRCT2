/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TestData.h"

#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/core/File.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectFactory.h>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::Drawing;

namespace
{

    // Helper: resolve an object path from environment or a sensible default
    std::optional<std::string> ResolveObjectPath()
    {
        const char* envPath = std::getenv("OPENRCT2_TEST_OBJECT_PATH");
        if (envPath != nullptr && *envPath != '\0')
        {
            if (OpenRCT2::File::Exists(envPath))
                return std::string(envPath);
            return std::nullopt;
        }

        // Fallback commonly used path on Linux
        const std::string fallback = std::string(getenv("HOME") ? getenv("HOME") : "") + "/.config/OpenRCT2/object/MTRBOAT.DAT";
        if (OpenRCT2::File::Exists(fallback))
            return fallback;

        return std::nullopt;
    }

    // Helper: fetch all RLE G1Elements from an object
    std::vector<G1Element> CollectRleSprites(OpenRCT2::Object& obj)
    {
        std::vector<G1Element> rles;
        const auto count = obj.GetNumImages();
        const auto* images = obj.GetImageTable().GetImages();
        for (uint32_t i = 0; i < count; i++)
        {
            const auto& g1 = images[i];
            if (g1.width > 0 && g1.height > 0 && g1.flags.has(G1Flag::hasRLECompression))
            {
                rles.push_back(g1);
            }
        }
        return rles;
    }

    struct RleDrawParam
    {
        int zoom; // 0..3
        int dx;   // X offset relative to 0 (can be negative)
        int dy;   // Y offset relative to 0 (can be negative)
        bool skipBoundsCheck = false;
    };

    class RleSpriteParamTests : public ::testing::TestWithParam<RleDrawParam>
    {
    protected:
        static std::unique_ptr<IContext> s_context;

        static void SetUpTestSuite()
        {
            // Avoid initialising any UI; run in headless mode
            gOpenRCT2Headless = true;
            s_context = CreateContext();
            s_context->Initialise();
        }

        static void TearDownTestSuite()
        {
            s_context.reset();
        }
    };

    std::unique_ptr<IContext> RleSpriteParamTests::s_context;

    TEST_P(RleSpriteParamTests, DrawsRleSpritesWithoutOverflow)
    {
        auto maybeObjPath = ResolveObjectPath();
        if (!maybeObjPath.has_value())
        {
            GTEST_SKIP() << "No object file found. Set OPENRCT2_TEST_OBJECT_PATH to a .DAT";
        }

        std::unique_ptr<Object> metaObject = OpenRCT2::ObjectFactory::CreateObjectFromFile(maybeObjPath->c_str(), true);
        if (metaObject == nullptr)
        {
            GTEST_SKIP() << "Unable to load object: " << *maybeObjPath;
        }

        const auto sprites = CollectRleSprites(*metaObject);
        if (sprites.empty())
        {
            GTEST_SKIP() << "Object has no RLE sprites: " << *maybeObjPath;
        }

        const auto p = GetParam();

        // Limit number of sprites to keep runtime reasonable (default: 64). Override via env.
        size_t maxSprites = std::min<size_t>(sprites.size(), 64);
        if (const char* envMax = std::getenv("OPENRCT2_TEST_MAX_SPRITES"); envMax && *envMax)
        {
            try
            {
                long v = std::strtol(envMax, nullptr, 10);
                if (v > 0)
                    maxSprites = std::min<size_t>(sprites.size(), static_cast<size_t>(v));
            }
            catch (...)
            {
            }
        }

        for (size_t idx = 0; idx < maxSprites; idx++)
        {
            const auto& g1 = sprites[idx];

            // Source region based on parameter offsets
            const int srcX = p.dx;
            const int srcY = p.dy;

            // Determine width/height of the region we ask to draw (clamped on positive side)
            const int reqW = std::max<int>(0, g1.width - std::max(0, srcX));
            const int reqH = std::max<int>(0, g1.height - std::max(0, srcY));
            if (reqW == 0 || reqH == 0)
                continue; // nothing to draw

            // Output dimensions depend on zoom level (minify)
            const int zoomFactor = 1 << p.zoom;
            const int outW = (reqW + zoomFactor - 1) >> p.zoom;
            const int outH = (reqH + zoomFactor - 1) >> p.zoom;

            std::vector<uint8_t> buffer;
            buffer.resize(static_cast<size_t>(outW) * outH, 0);

            auto* paletteBits = reinterpret_cast<OpenRCT2::Drawing::PaletteIndex*>(buffer.data());
            OpenRCT2::Drawing::RenderTarget rt;
            rt.bits = paletteBits;
            rt.x = 0;
            rt.y = 0;
            rt.width = outW;
            rt.height = outH;
            rt.pitch = 0;
            rt.zoom_level = ZoomLevel{ static_cast<int8_t>(p.zoom) };

            DrawSpriteArgs args(ImageId(), PaletteMap::GetDefault(), g1, srcX, srcY, reqW, reqH, paletteBits);

            // Use bounds-checking version to validate sprite data
            bool result = GfxRleSpriteToBufferWithBoundsCheck(rt, args);
            EXPECT_TRUE(result) << "Sprite data bounds violation at sprite index: " << idx;
        }
    }

    INSTANTIATE_TEST_SUITE_P(
        ZoomAndOffsetCases, RleSpriteParamTests,
        ::testing::Values(
            RleDrawParam{ 0, 0, 0 }, RleDrawParam{ 0, -1, 0 }, RleDrawParam{ 0, 0, -1 }, RleDrawParam{ 0, 1, 1 },
            RleDrawParam{ 1, 0, 0 }, RleDrawParam{ 1, -1, -1 }, RleDrawParam{ 1, 1, 0 }, RleDrawParam{ 2, 0, 1 },
            RleDrawParam{ 2, -1, 0 }, RleDrawParam{ 3, 0, -1 }));

    TEST(RleSpriteTests, ValidRleDataPasses)
    {
        // 2x2 image, valid data
        std::vector<uint8_t> data = {
            0x04, 0x00, // Line 0 offset (4)
            0x08, 0x00, // Line 1 offset (8)
            2, 0, 0x11, 0x22, // Line 0: 2 pixels at 0: 0x11, 0x22
            2 | 0x80, 0, 0x33, 0x44 // Line 1: 2 pixels at 0: 0x33, 0x44 (end of line)
        };

        G1Element g1{};
        g1.width = 2;
        g1.height = 2;
        g1.offset = data.data();
        g1.size = static_cast<uint32_t>(data.size());
        g1.flags = G1Flag::hasRLECompression;

        std::vector<uint8_t> buffer(4);
        auto* paletteBits = reinterpret_cast<OpenRCT2::Drawing::PaletteIndex*>(buffer.data());
        OpenRCT2::Drawing::RenderTarget rt;
        rt.bits = paletteBits;
        rt.width = 2;
        rt.height = 2;
        rt.pitch = 0;
        rt.zoom_level = ZoomLevel{ 0 };

        DrawSpriteArgs args(ImageId(), PaletteMap::GetDefault(), g1, 0, 0, 2, 2, paletteBits);

        bool result = GfxRleSpriteToBufferWithBoundsCheck(rt, args);
        EXPECT_TRUE(result) << "Valid RLE data should pass validation";
    }

    TEST(RleSpriteTests, MalformedRleDataDetected)
    {
        // 1x1 image, but data says it has a run of 10 pixels, and we only provide 2 bytes of data
        std::vector<uint8_t> data = {
            0x02, 0x00, // Line 0 offset (2)
            10, 0,      // 10 pixels, start at 0
            0xAA, 0xBB  // Only 2 pixels provided
        };

        G1Element g1{};
        g1.width = 10;
        g1.height = 1;
        g1.offset = data.data();
        g1.size = static_cast<uint32_t>(data.size());
        g1.flags = G1Flag::hasRLECompression;

        std::vector<uint8_t> buffer(10);
        auto* paletteBits = reinterpret_cast<OpenRCT2::Drawing::PaletteIndex*>(buffer.data());
        OpenRCT2::Drawing::RenderTarget rt;
        rt.bits = paletteBits;
        rt.width = 10;
        rt.height = 1;
        rt.pitch = 0;
        rt.zoom_level = ZoomLevel{ 0 };

        DrawSpriteArgs args(ImageId(), PaletteMap::GetDefault(), g1, 0, 0, 10, 1, paletteBits);

        bool result = GfxRleSpriteToBufferWithBoundsCheck(rt, args);
        EXPECT_FALSE(result) << "Malformed RLE data (run exceeds buffer) should be detected";
    }

    TEST(RleSpriteTests, MagnificationBoundsCheck)
    {
        // Malformed data: Line 0 offset points beyond buffer
        std::vector<uint8_t> data = {
            0x10, 0x00 // Line 0 offset (16), but data size is only 2
        };

        G1Element g1{};
        g1.width = 10;
        g1.height = 1;
        g1.offset = data.data();
        g1.size = static_cast<uint32_t>(data.size());
        g1.flags = G1Flag::hasRLECompression;

        std::vector<uint8_t> buffer(40);
        auto* paletteBits = reinterpret_cast<OpenRCT2::Drawing::PaletteIndex*>(buffer.data());
        OpenRCT2::Drawing::RenderTarget rt;
        rt.bits = paletteBits;
        rt.width = 20;
        rt.height = 2;
        rt.pitch = 0;
        rt.zoom_level = ZoomLevel{ -1 }; // 2x zoom

        DrawSpriteArgs args(ImageId(), PaletteMap::GetDefault(), g1, 0, 0, 10, 1, paletteBits);

        bool result = GfxRleSpriteToBufferWithBoundsCheck(rt, args);
        EXPECT_FALSE(result) << "Malformed RLE data (offset out of bounds) should be detected in magnification";
    }

    TEST(RleSpriteTests, MtrBoatFailsValidation)
    {
        // Initialize context
        gOpenRCT2Headless = true;
        auto context = CreateContext();
        context->Initialise();

        auto path = OpenRCT2::Path::Combine(TestData::GetBasePath(), "objects", "MTRBOAT.DAT");
        if (!OpenRCT2::File::Exists(path))
        {
            GTEST_SKIP() << "MTRBOAT.DAT not found in testdata";
        }

        std::unique_ptr<Object> obj = OpenRCT2::ObjectFactory::CreateObjectFromFile(path, true);
        EXPECT_EQ(obj, nullptr) << "MTRBOAT.DAT should fail RLE validation and not be loaded";
    }

} // namespace
