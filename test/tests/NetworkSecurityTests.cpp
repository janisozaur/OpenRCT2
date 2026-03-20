/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/general/TileModifyAction.h>
#include <openrct2/network/NetworkPacket.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Map.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Network;

TEST(NetworkPacket, ReadString_NullTerminated)
{
    Packet packet(Command::chat);
    const char* testStr = "Hello World";
    packet.WriteString(testStr);

    // Header size includes the null terminator
    packet.Header.size = static_cast<uint32_t>(packet.Data.size());

    std::string_view readStr = packet.ReadString();
    ASSERT_EQ(readStr, testStr);
}

TEST(NetworkPacket, ReadString_NoNullTerminator)
{
    Packet packet(Command::chat);
    const char* testStr = "Malformed";
    packet.Write(testStr, strlen(testStr));

    packet.Header.size = static_cast<uint32_t>(packet.Data.size());

    std::string_view readStr = packet.ReadString();
    ASSERT_TRUE(readStr.empty());
}

TEST(NetworkPacket, ReadString_Empty)
{
    Packet packet(Command::chat);
    packet.Header.size = 0;

    std::string_view readStr = packet.ReadString();
    ASSERT_TRUE(readStr.empty());
}

TEST(NetworkPacket, ReadString_OnlyNull)
{
    Packet packet(Command::chat);
    uint8_t zero = 0;
    packet.Write(&zero, 1);
    packet.Header.size = 1;

    std::string_view readStr = packet.ReadString();
    ASSERT_EQ(readStr.size(), 0);
    ASSERT_EQ(packet.BytesRead, 1);
}

TEST(TileModifyAction, ValidatePasteElement_InvalidBannerIndex)
{
    gOpenRCT2Headless = true;
    gOpenRCT2NoGraphics = true;

    // Attempt to set data path so context can find language files
    gCustomOpenRCT2DataPath = "../data";

    auto context = CreateContext();
    if (!context->Initialise())
    {
        // Try current directory data path if root failed
        gCustomOpenRCT2DataPath = "data";
        ASSERT_TRUE(context->Initialise());
    }

    MapInit({ 10, 10 });

    TileElement element;
    element.ClearAs(TileElementType::Banner);
    element.SetBannerIndex(BannerIndex::FromUnderlying(kMaxBanners));

    GameActions::TileModifyAction action({ 32, 32 }, GameActions::TileModifyType::AnyPaste, 0, 0, element);

    auto result = action.Query(getGameState());
    ASSERT_EQ(result.error, GameActions::Status::invalidParameters);
}
