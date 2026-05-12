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
#include <openrct2/network/INetworkLogger.h>
#include <openrct2/network/INetworkPlatform.h>
#include <openrct2/network/NetworkBase.h>
#include <openrct2/network/NetworkGroup.h>
#include <openrct2/network/NetworkUser.h>

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
        return "localhost";
    }
    std::string GetIpAddress() const override
    {
        return "127.0.0.1";
    }
    void Listen(uint16_t port) override
    {
    }
    void Listen(const std::string& address, uint16_t port) override
    {
    }
    std::unique_ptr<ITcpSocket> Accept() override
    {
        return nullptr;
    }
    void Connect(const std::string& address, uint16_t port) override
    {
    }
    void ConnectAsync(const std::string& address, uint16_t port) override
    {
    }
    size_t SendData(const void* buffer, size_t size) override
    {
        return size;
    }
    ReadPacket ReceiveData(void* buffer, size_t size, size_t* sizeReceived) override
    {
        return ReadPacket::noData;
    }
    void SetNoDelay(bool noDelay) override
    {
    }
    void Finish() override
    {
    }
    void Disconnect() override
    {
    }
    void Close() override
    {
    }
};

class MockNetworkLogger final : public INetworkLogger
{
public:
    void BeginChatLog() override
    {
    }
    void AppendChatLog(std::string_view s) override
    {
    }
    void CloseChatLog() override
    {
    }
    void BeginServerLog(const std::string& serverName, bool isClient) override
    {
    }
    void AppendServerLog(std::string_view s) override
    {
    }
    void CloseServerLog(bool isClient) override
    {
    }
};

class MockNetworkPlatform final : public INetworkPlatform
{
public:
    std::unique_ptr<ITcpSocket> CreateTcpSocket() override
    {
        return std::make_unique<MockTcpSocket>();
    }
    std::unique_ptr<IUdpSocket> CreateUdpSocket() override
    {
        return nullptr;
    }
    std::unique_ptr<INetworkServerAdvertiser> CreateServerAdvertiser(uint16_t port) override
    {
        return nullptr;
    }
    bool LoadPrivateKey(const std::string& playerName, Key& key) override
    {
        return true;
    }
    bool SavePrivateKey(const std::string& playerName, const Key& key) override
    {
        return true;
    }
    bool SavePublicKey(const std::string& playerName, const Key& key) override
    {
        return true;
    }
    void GenerateKey(Key& key) override
    {
    }
    void LoadGroups(std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t& defaultGroup) override
    {
        auto admin = std::make_unique<NetworkGroup>();
        admin->Id = 0;
        admin->SetName("Admin");
        groups.push_back(std::move(admin));
        defaultGroup = 0;
    }
    void SaveGroups(const std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t defaultGroup) override
    {
    }
    void LoadUserManager(UserManager& userManager) override
    {
    }
    void SaveUserManager(const UserManager& userManager) override
    {
    }
};

TEST(NetworkTests, BeginServerBypassesActualNetwork)
{
    auto context = CreateContext();
    // We need to initialize the context so that ObjectRepository is created,
    // which is needed for some network handlers.
    // However, Initialise() might try to load RCT2 data.
    // For unit tests, we might need a more lightweight way or a mock context.

    // Given the task's example:
    // NetworkBase network(*_context);
    // network.BeginServer(0, "127.0.0.1");

    auto platform = std::make_unique<MockNetworkPlatform>();
    auto logger = std::make_unique<MockNetworkLogger>();
    NetworkBase network(*context, std::move(platform), std::move(logger));

    // This should now be fast and not touch real sockets or files
    bool success = network.BeginServer(0, "127.0.0.1");
    EXPECT_TRUE(success);
    EXPECT_EQ(network.GetMode(), Mode::server);
    EXPECT_EQ(network.GetStatus(), Status::connected);
}
