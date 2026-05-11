/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/network/NetworkPacket.h>

using namespace OpenRCT2::Network;

TEST(NetworkTests, MapRequestRequiresAuth)
{
    Packet packet(Command::mapRequest);
    EXPECT_TRUE(packet.CommandRequiresAuth());
}

TEST(NetworkTests, OtherCommandsRequireAuth)
{
    Packet chatPacket(Command::chat);
    EXPECT_TRUE(chatPacket.CommandRequiresAuth());

    Packet gameActionPacket(Command::gameAction);
    EXPECT_TRUE(gameActionPacket.CommandRequiresAuth());
}

TEST(NetworkTests, SomeCommandsDoNotRequireAuth)
{
    Packet pingPacket(Command::ping);
    EXPECT_FALSE(pingPacket.CommandRequiresAuth());

    Packet authPacket(Command::auth);
    EXPECT_FALSE(authPacket.CommandRequiresAuth());

    Packet gameInfoPacket(Command::gameInfo);
    EXPECT_FALSE(gameInfoPacket.CommandRequiresAuth());
}
