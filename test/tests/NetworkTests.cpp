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
        return _status;
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
        _status = SocketStatus::listening;
    }
    void Listen(const std::string&, uint16_t) override
    {
        _status = SocketStatus::listening;
    }
    std::unique_ptr<ITcpSocket> Accept() override
    {
        return nullptr;
    }

    void Connect(const std::string&, uint16_t) override
    {
        _status = SocketStatus::connected;
    }
    void ConnectAsync(const std::string&, uint16_t) override
    {
        _status = SocketStatus::connecting;
    }

    void SetInboundData(const std::vector<uint8_t>& data)
    {
        _inboundData = data;
        _inboundOffset = 0;
    }

    size_t SendData(const void*, size_t size) override
    {
        return size;
    }
    ReadPacket ReceiveData(void* buffer, size_t size, size_t* sizeReceived) override
    {
        if (_inboundOffset >= _inboundData.size())
        {
            *sizeReceived = 0;
            return ReadPacket::noData;
        }
        size_t available = _inboundData.size() - _inboundOffset;
        size_t toCopy = std::min(size, available);
        std::memcpy(buffer, _inboundData.data() + _inboundOffset, toCopy);
        _inboundOffset += toCopy;
        *sizeReceived = toCopy;
        return ReadPacket::success;
    }

    void SetNoDelay(bool) override
    {
    }

    void Finish() override
    {
    }
    void Disconnect() override
    {
        _status = SocketStatus::closed;
    }
    void Close() override
    {
        _status = SocketStatus::closed;
    }

private:
    SocketStatus _status = SocketStatus::connected;
    std::vector<uint8_t> _inboundData;
    size_t _inboundOffset = 0;
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
    connection.Socket = std::move(mockSocket);
    connection.AuthStatus = Auth::none;
    connection.player = nullptr;

    Packet packet(Command::mapRequest);
    packet << static_cast<uint32_t>(0); // 0 objects

    // Before the fix, this would crash because it accesses connection.player->Name
    // After the fix, it should detect connection.player == nullptr and disconnect.
    network.ServerHandleMapRequest(connection, packet);

    EXPECT_TRUE(connection.ShouldDisconnect);
}

TEST_F(NetworkTests, ProcessPacketInterceptsUnauthorizedCommands)
{
    NetworkBase network(*_context);

    Connection connection;
    connection.Socket = std::make_unique<MockTcpSocket>();
    connection.AuthStatus = Auth::none;

    // Chat command requires authentication
    Packet packet(Command::chat);
    packet.WriteString("Hello");

    // ProcessPacket should check CommandRequiresAuth and disconnect because AuthStatus != Auth::ok
    network.ProcessPacket(connection, packet);

    EXPECT_TRUE(connection.ShouldDisconnect);
}

TEST_F(NetworkTests, IntegratedUnauthenticatedMapRequestDisconnects)
{
    // This test simulates receiving a mapRequest packet from the network
    // while unauthenticated, and verifies that the server disconnects the client.
    NetworkBase network(*_context);

    // Set network to server mode
    network.BeginServer(0, "127.0.0.1");

    Connection connection;
    auto mockSocket = std::make_unique<MockTcpSocket>();

    // Manually construct a mapRequest packet in ORT2 format.
    // Magic 'ORT2' is 0x3254524F in host order (Little Endian).
    // On the wire it is expected to be Network Order (Big Endian).
    // So 0x3254524F -> HostToNetwork -> 0x4F525432.
    // Bytes of 0x4F525432 on a Little Endian machine are 32 54 52 4F.
    std::vector<uint8_t> bytes = {
        0x32, 0x54, 0x52, 0x4F, // Magic (corresponds to 0x4F525432 in memory on LE, which swaps to 0x3254524F)
        0x00, 0x02,             // Version 2 (Big Endian)
        0x00, 0x00, 0x00, 0x04, // Size 4 (Big Endian)
        0x00, 0x00, 0x00, 0x0F, // Id 15 (mapRequest, Big Endian)
        0x00, 0x00, 0x00, 0x00  // Data: 0 objects
    };

    mockSocket->SetInboundData(bytes);
    connection.Socket = std::move(mockSocket);
    connection.AuthStatus = Auth::none;

    // 1. Receive data and fill inbound buffer
    connection.update();

    // 2. Process connection (reads packet and processes it)
    network.ProcessConnection(connection);

    // Should be disconnected because mapRequest requires authentication now.
    EXPECT_TRUE(connection.ShouldDisconnect);
}

TEST_F(NetworkTests, MapRequestRequiresAuth)
{
    Packet packet(Command::mapRequest);
    EXPECT_TRUE(packet.CommandRequiresAuth());
}
