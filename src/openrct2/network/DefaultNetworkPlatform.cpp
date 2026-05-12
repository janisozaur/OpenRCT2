/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_NETWORK

    #include "DefaultNetworkPlatform.h"

    #include "../Context.h"
    #include "../PlatformEnvironment.h"
    #include "../core/File.h"
    #include "../core/FileStream.h"
    #include "../core/Json.hpp"
    #include "../core/Path.hpp"
    #include "NetworkGroup.h"
    #include "NetworkUser.h"

namespace OpenRCT2::Network
{
    DefaultNetworkPlatform::DefaultNetworkPlatform(IContext& context)
        : _context(context)
    {
    }

    std::unique_ptr<ITcpSocket> DefaultNetworkPlatform::CreateTcpSocket()
    {
        return OpenRCT2::Network::CreateTcpSocket();
    }

    std::unique_ptr<IUdpSocket> DefaultNetworkPlatform::CreateUdpSocket()
    {
        return OpenRCT2::Network::CreateUdpSocket();
    }

    std::unique_ptr<INetworkServerAdvertiser> DefaultNetworkPlatform::CreateServerAdvertiser(uint16_t port)
    {
        return OpenRCT2::Network::CreateServerAdvertiser(port);
    }

    bool DefaultNetworkPlatform::LoadPrivateKey(const std::string& playerName, Key& key)
    {
        const auto keyPath = GetPrivateKeyPath(playerName);
        if (!File::Exists(keyPath))
        {
            return false;
        }

        auto fs = FileStream(keyPath, FileMode::open);
        return key.LoadPrivate(&fs);
    }

    bool DefaultNetworkPlatform::SavePrivateKey(const std::string& playerName, const Key& key)
    {
        const auto keysDirectory = GetKeysDirectory();
        if (!Path::CreateDirectory(keysDirectory))
        {
            return false;
        }

        const auto keyPath = GetPrivateKeyPath(playerName);
        auto fs = FileStream(keyPath, FileMode::write);
        const_cast<Key&>(key).SavePrivate(&fs);
        return true;
    }

    bool DefaultNetworkPlatform::SavePublicKey(const std::string& playerName, const Key& key)
    {
        const auto keysDirectory = GetKeysDirectory();
        if (!Path::CreateDirectory(keysDirectory))
        {
            return false;
        }

        const auto hash = const_cast<Key&>(key).PublicKeyHash();
        const auto keyPath = GetPublicKeyPath(playerName, hash);
        auto fs = FileStream(keyPath, FileMode::write);
        const_cast<Key&>(key).SavePublic(&fs);
        return true;
    }

    void DefaultNetworkPlatform::GenerateKey(Key& key)
    {
        key.Generate();
    }

    void DefaultNetworkPlatform::LoadGroups(std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t& defaultGroup)
    {
        groups.clear();

        auto& env = _context.GetPlatformEnvironment();
        auto path = Path::Combine(env.GetDirectoryPath(DirBase::user), "groups.json");

        json_t jsonGroupConfig;
        if (File::Exists(path))
        {
            try
            {
                jsonGroupConfig = Json::ReadFromFile(path);
            }
            catch (const std::exception&)
            {
            }
        }

        if (jsonGroupConfig.is_object())
        {
            json_t jsonGroups = jsonGroupConfig["groups"];
            if (jsonGroups.is_array())
            {
                for (auto& jsonGroup : jsonGroups)
                {
                    groups.emplace_back(std::make_unique<NetworkGroup>(NetworkGroup::FromJson(jsonGroup)));
                }
            }

            defaultGroup = Json::GetNumber<uint8_t>(jsonGroupConfig["default_group"]);
        }
    }

    void DefaultNetworkPlatform::SaveGroups(const std::vector<std::unique_ptr<NetworkGroup>>& groups, uint8_t defaultGroup)
    {
        auto& env = _context.GetPlatformEnvironment();
        auto path = Path::Combine(env.GetDirectoryPath(DirBase::user), "groups.json");

        json_t jsonGroups = json_t::array();
        for (auto& group : groups)
        {
            jsonGroups.push_back(group->ToJson());
        }
        json_t jsonGroupsCfg = {
            { "default_group", defaultGroup },
            { "groups", jsonGroups },
        };
        try
        {
            Json::WriteToFile(path, jsonGroupsCfg);
        }
        catch (const std::exception&)
        {
        }
    }

    void DefaultNetworkPlatform::LoadUserManager(UserManager& userManager)
    {
        userManager.Load();
    }

    void DefaultNetworkPlatform::SaveUserManager(const UserManager& userManager)
    {
        const_cast<UserManager&>(userManager).Save();
    }

    std::string DefaultNetworkPlatform::GetKeysDirectory()
    {
        auto& env = _context.GetPlatformEnvironment();
        return Path::Combine(env.GetDirectoryPath(DirBase::user), "keys");
    }

    std::string DefaultNetworkPlatform::GetPrivateKeyPath(const std::string& playerName)
    {
        return Path::Combine(GetKeysDirectory(), playerName + ".privkey");
    }

    std::string DefaultNetworkPlatform::GetPublicKeyPath(const std::string& playerName, const std::string& hash)
    {
        const auto filename = playerName + "-" + hash + ".pubkey";
        return Path::Combine(GetKeysDirectory(), filename);
    }
} // namespace OpenRCT2::Network

#endif
