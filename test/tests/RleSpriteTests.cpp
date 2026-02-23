/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TestData.h"

#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/core/File.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/drawing/Drawing.Sprite.h>
#include <openrct2/object/ImageTable.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectFactory.h>

using namespace OpenRCT2;

class RleSpriteTests : public testing::TestWithParam<std::pair<std::string, int8_t>>
{
protected:
    std::unique_ptr<IContext> context;

    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = false;
        context = CreateContext();
        ASSERT_NE(context, nullptr);
    }

    void TearDown() override
    {
        context.reset();
    }

    void ValidateRleObjectBuffer(const uint8_t* data, size_t size)
    {
        if (size < sizeof(RCTObjectEntry))
            return;

        auto* entry = reinterpret_cast<const RCTObjectEntry*>(data);
        [[maybe_unused]] auto object = ObjectFactory::CreateObjectFromLegacyData(
            entry, data + sizeof(RCTObjectEntry), size - sizeof(RCTObjectEntry));
    }
};

TEST_P(RleSpriteTests, ValidateRleObject)
{
    auto [filename, zoom] = GetParam();
    std::string path = OpenRCT2::Path::Combine(TestData::GetBasePath(), filename.c_str());

    auto object = ObjectFactory::CreateObjectFromFile(path, true);
    if (filename == "MTRBOAT.DAT")
    {
        ASSERT_EQ(object, nullptr) << "MTRBOAT.DAT should fail to load due to invalid RLE data";
    }
    else
    {
        ASSERT_NE(object, nullptr) << "Failed to load object from " << path;
    }
}

TEST_F(RleSpriteTests, ValidateRleObjectFromMemory)
{
    std::string path = OpenRCT2::Path::Combine(TestData::GetBasePath(), "MTRBOAT.DAT");
    auto data = OpenRCT2::File::ReadAllBytes(path);

    ValidateRleObjectBuffer(data.data(), data.size());
}

INSTANTIATE_TEST_SUITE_P(Objects, RleSpriteTests, testing::Values(std::make_pair("MTRBOAT.DAT", 0)));
