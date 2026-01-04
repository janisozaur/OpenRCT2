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

} // namespace
