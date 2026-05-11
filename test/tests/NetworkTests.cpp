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
#include <openrct2/OpenRCT2.h>
#include <openrct2/network/NetworkBase.h>
#include <openrct2/network/NetworkConnection.h>
#include <openrct2/network/NetworkPacket.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Network;

class MockTcpSocket final : public ITcpSocket
{
public:
    SocketStatus GetStatus() const override
    {
        return SocketStatus::connected;
    }
    const char* GetError() const override
    {
        return nullptr;
    }
    const char* GetHostName() const override
    {
        return "127.0.0.1";
    }
    std::string GetIpAddress() const override
    {
        return "127.0.0.1";
    }

    void Listen(uint16_t) override
    {
    }
    void Listen(const std::string&, uint16_t) override
    {
    }
    std::unique_ptr<ITcpSocket> Accept() override
    {
        return nullptr;
    }

    void Connect(const std::string&, uint16_t) override
    {
    }
    void ConnectAsync(const std::string&, uint16_t) override
    {
    }

    size_t SendData(const void*, size_t size) override
    {
        return size;
    }
    ReadPacket ReceiveData(void*, size_t, size_t*) override
    {
        return ReadPacket::noData;
    }

    void SetNoDelay(bool) override
    {
    }

    void Finish() override
    {
    }
    void Disconnect() override
    {
        _disconnected = true;
    }
    void Close() override
    {
        _disconnected = true;
    }

    bool IsDisconnected() const
    {
        return _disconnected;
    }

private:
    bool _disconnected = false;
};

class NetworkTests : public testing::Test
{
protected:
    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        try
        {
            _context->Initialise();
        }
        catch (...)
        {
        }
    }

    std::unique_ptr<IContext> _context;
};

TEST_F(NetworkTests, MapRequestWithoutPlayerDisconnectsAndDoesNotCrash)
{
    NetworkBase network(*_context);

    Connection connection;
    auto mockSocket = std::make_unique<MockTcpSocket>();
    auto* mockSocketPtr = mockSocket.get();
    connection.Socket = std::move(mockSocket);
    connection.AuthStatus = Auth::none;
    connection.player = nullptr;

    Packet packet(Command::mapRequest);
    packet << static_cast<uint32_t>(0); // 0 objects

    // Before the fix, this would crash because it accesses connection.player->Name
    // After the fix, it should detect connection.player == nullptr and disconnect.
    network.ServerHandleMapRequest(connection, packet);

    EXPECT_TRUE(mockSocketPtr->IsDisconnected() || connection.ShouldDisconnect);
}

TEST_F(NetworkTests, ProcessPacketInterceptsUnauthorizedCommands)
{
    NetworkBase network(*_context);

    Connection connection;
    auto mockSocket = std::make_unique<MockTcpSocket>();
    auto* mockSocketPtr = mockSocket.get();
    connection.Socket = std::move(mockSocket);
    connection.AuthStatus = Auth::none;

    // Chat command requires authentication
    Packet packet(Command::chat);
    packet.WriteString("Hello");

    // ProcessPacket should check CommandRequiresAuth and disconnect because AuthStatus != Auth::ok
    network.ProcessPacket(connection, packet);

    EXPECT_TRUE(mockSocketPtr->IsDisconnected() || connection.ShouldDisconnect);
}

TEST_F(NetworkTests, MapRequestRequiresAuth)
{
    Packet packet(Command::mapRequest);
    EXPECT_TRUE(packet.CommandRequiresAuth());
}
