/*****************************************************************************
 * Copyright (c) 2014-2025 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <fstream>
#include <gtest/gtest.h>
#include <openrct2/platform/LibuvLoop.h>
#include <openrct2/platform/Platform.h>

using namespace OpenRCT2;

TEST(platform, sanitise_filename)
{
    ASSERT_EQ("normal-filename.png", Platform::SanitiseFilename("normal-filename.png"));
    ASSERT_EQ("utf🎱", Platform::SanitiseFilename("utf🎱"));
    ASSERT_EQ("forbidden_char", Platform::SanitiseFilename("forbidden/char"));
    ASSERT_EQ("non trimmed", Platform::SanitiseFilename(" non trimmed "));
#ifndef _WIN32
    ASSERT_EQ("forbidden_\\:\"|?*chars", Platform::SanitiseFilename("forbidden/\\:\"|?*chars"));
#else
    ASSERT_EQ("forbidden_______chars", Platform::SanitiseFilename("forbidden/\\:\"|?*chars"));
#endif
}

TEST(platform, read_all_bytes_async)
{
    const std::string testFile = "test_async.bin";
    const std::vector<uint8_t> testData = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04 };

    // Create test file
    std::ofstream ofs(testFile, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(testData.data()), testData.size());
    ofs.close();

    bool done = false;
    std::vector<uint8_t> readData;
    Platform::ReadAllBytesAsync(testFile, [&](std::vector<uint8_t>&& data) {
        readData = std::move(data);
        done = true;
    });

    // Tick the loop
    int iterations = 0;
    while (!done && iterations < 1000)
    {
#ifdef USE_LIBUV
        Platform::LibuvLoop::Get().Tick();
#endif
        Platform::Sleep(1);
        iterations++;
    }

    EXPECT_TRUE(done);
    EXPECT_EQ(testData, readData);

    // Cleanup
    std::remove(testFile.c_str());
}
