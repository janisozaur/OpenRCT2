/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "INetworkPlatform.h"

namespace OpenRCT2
{
    struct IContext;
}

namespace OpenRCT2::Network
{
    class DefaultNetworkPlatform final : public INetworkPlatform
    {
    private:
        IContext& _context;

    public:
        DefaultNetworkPlatform(IContext& context);

        std::unique_ptr<ITcpSocket> CreateTcpSocket() override;
        std::unique_ptr<IUdpSocket> CreateUdpSocket() override;
        std::unique_ptr<INetworkServerAdvertiser> CreateServerAdvertiser(uint16_t port) override;

        bool LoadPrivateKey(const std::string& playerName, Key& key) override;
        bool SavePrivateKey(const std::string& playerName, const Key& key) override;
        bool SavePublicKey(const std::string& playerName, const Key& key) override;
        void GenerateKey(Key& key) override;

        void LoadGroups(std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t& defaultGroup) override;
        void SaveGroups(const std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t defaultGroup) override;

        void LoadUserManager(UserManager& userManager) override;
        void SaveUserManager(const UserManager& userManager) override;

    private:
        std::string GetKeysDirectory();
        std::string GetPrivateKeyPath(const std::string& playerName);
        std::string GetPublicKeyPath(const std::string& playerName, const std::string& hash);
    };
} // namespace OpenRCT2::Network
