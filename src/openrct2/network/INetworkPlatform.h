/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "NetworkKey.h"
#include "NetworkServerAdvertiser.h"
#include "Socket.h"

#include <memory>
#include <string>
#include <vector>

namespace OpenRCT2::Network
{
    class NetworkGroup;
    class UserManager;

    struct INetworkPlatform
    {
        virtual ~INetworkPlatform() = default;

        virtual std::unique_ptr<ITcpSocket> CreateTcpSocket() = 0;
        virtual std::unique_ptr<IUdpSocket> CreateUdpSocket() = 0;
        virtual std::unique_ptr<INetworkServerAdvertiser> CreateServerAdvertiser(uint16_t port) = 0;

        virtual bool LoadPrivateKey(const std::string& playerName, Key& key) = 0;
        virtual bool SavePrivateKey(const std::string& playerName, const Key& key) = 0;
        virtual bool SavePublicKey(const std::string& playerName, const Key& key) = 0;
        virtual void GenerateKey(Key& key) = 0;

        virtual void LoadGroups(std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t& defaultGroup) = 0;
        virtual void SaveGroups(const std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t defaultGroup) = 0;

        virtual void LoadUserManager(UserManager& userManager) = 0;
        virtual void SaveUserManager(const UserManager& userManager) = 0;
    };
} // namespace OpenRCT2::Network
