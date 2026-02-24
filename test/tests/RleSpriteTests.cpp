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
#include <openrct2/core/MemoryStream.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/drawing/Drawing.Sprite.h>
#include <openrct2/object/ImageTable.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectFactory.h>

using namespace OpenRCT2;

class MockReadObjectContext : public IReadObjectContext
{
public:
    std::vector<std::pair<ObjectError, std::string>> Errors;
    std::vector<std::pair<ObjectError, std::string>> Warnings;

    std::string_view GetObjectIdentifier() override
    {
        return "MOCK";
    }
    bool ShouldLoadImages() override
    {
        return true;
    }
    std::vector<uint8_t> GetData(std::string_view) override
    {
        return {};
    }
    ObjectAsset GetAsset(std::string_view) override
    {
        return {};
    }

    void LogVerbose(ObjectError, const utf8*) override
    {
    }
    void LogWarning(ObjectError code, const utf8* text) override
    {
        Warnings.push_back({ code, text ? text : "" });
    }
    void LogError(ObjectError code, const utf8* text) override
    {
        Errors.push_back({ code, text ? text : "" });
    }
};

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

    void ValidateRleObjectBuffer(const uint8_t* data, size_t size, MockReadObjectContext& mockContext)
    {
        if (size < sizeof(RCTObjectEntry))
            return;

        auto* entry = reinterpret_cast<const RCTObjectEntry*>(data);
        auto object = ObjectFactory::CreateObject(entry->GetType());
        if (!object)
            return;

        MemoryStream ms(data + sizeof(RCTObjectEntry), size - sizeof(RCTObjectEntry));
        try
        {
            object->ReadLegacy(&mockContext, &ms);
        }
        catch (const std::exception&)
        {
            // ReadLegacy might throw if it hits an error we added
        }
    }
};

TEST_P(RleSpriteTests, ValidateRleObject)
{
    auto [filename, zoom] = GetParam();
    std::string path = OpenRCT2::Path::Combine(TestData::GetBasePath(), filename.c_str());

    auto data = OpenRCT2::File::ReadAllBytes(path);
    MockReadObjectContext mockContext;
    ValidateRleObjectBuffer(data.data(), data.size(), mockContext);

    if (filename == "MTRBOAT.DAT")
    {
        size_t imageErrorCount = 0;
        for (const auto& err : mockContext.Errors)
        {
            if (err.first == ObjectError::badImageTable)
            {
                imageErrorCount++;
            }
        }
        EXPECT_GT(imageErrorCount, 0u) << "MTRBOAT.DAT should have at least one bad image table error";
    }
    else
    {
        EXPECT_EQ(mockContext.Errors.size(), 0u);
    }
}

TEST_F(RleSpriteTests, ValidateRleObjectFromMemory)
{
    std::string path = OpenRCT2::Path::Combine(TestData::GetBasePath(), "MTRBOAT.DAT");
    auto data = OpenRCT2::File::ReadAllBytes(path);

    MockReadObjectContext mockContext;
    ValidateRleObjectBuffer(data.data(), data.size(), mockContext);

    bool foundImageError = false;
    for (const auto& err : mockContext.Errors)
    {
        if (err.first == ObjectError::badImageTable)
        {
            foundImageError = true;
            break;
        }
    }
    EXPECT_TRUE(foundImageError);
}

INSTANTIATE_TEST_SUITE_P(Objects, RleSpriteTests, testing::Values(std::make_pair("MTRBOAT.DAT", 0)));
