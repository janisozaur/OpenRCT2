#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#ifndef DISABLE_NETWORK

#include <vector>
#include "../core/String.hpp"
#include "NetworkGroupManager.h"
#include "NetworkPlayer.h"
#include "NetworkPlayerManager.h"
#include "NetworkUser.h"
#include "NetworkUserManager.h"

class NetworkPlayerManager : public INetworkPlayerManager
{
private:
    INetworkGroupManager *          _groupManager;
    INetworkUserManager *           _userManager;
    uint32                          _maxPlayers;
    std::vector<NetworkPlayer *>    _players;

public:
    NetworkPlayerManager(INetworkGroupManager * groupManager, INetworkUserManager * userManager)
    {
        _groupManager = groupManager;
        _userManager = userManager;
    }

    virtual ~NetworkPlayerManager() { }

    uint32 GetCount() const override
    {
        return _players.size();
    }

    bool IsFull() const override
    {
        return _players.size() >= _maxPlayers;
    }

    NetworkPlayer * GetPlayerById(uint8 id) const
    {
        for (auto player : _players)
        {
            if (id == player->id)
            {
                return player;
            }
        }
        return nullptr;
    }

    NetworkPlayer * GetPlayerByIndex(uint32 index) const
    {
        if (index >= _players.size()) return nullptr;
        return _players[index];
    }

    NetworkPlayer * CreatePlayer(const utf8 * name, const char * hash) override
    {
        NetworkPlayer * player = nullptr;

        uint8 nextId;
        if (TryGetNextPlayerId(&nextId))
        {
            // Load keys host may have added manually
            _userManager->Load();

            // Check if the key is registered
            const NetworkUser * networkUser = _userManager->GetUserByHash(hash);

            player = new NetworkPlayer();
            player->id = nextId;
            player->keyhash = hash;
            if (networkUser == nullptr)
            {
                player->group = _groupManager->GetDefaultGroupId();
                if (!String::IsNullOrEmpty(name))
                {
                    player->SetName(MakePlayerNameUnique(std::string(name)));
                }
            }
            else
            {
                uint8 defaultGroupId = _groupManager->GetDefaultGroupId();
                player->group = networkUser->GroupId.GetValueOrDefault(defaultGroupId);
                player->SetName(networkUser->Name);
            }
            _players.push_back(player);
        }
        return player;
    }

    void Clear() override
    {
        _players.clear();
    }

    void Remove(NetworkPlayer * player) override
    {
        auto it = std::find(_players.begin(), _players.end(), player);
        if (it != _players.end())
        {
            _players.erase(it);
        }
    }

private:
    bool TryGetNextPlayerId(uint8 * outId) const
    {
        for (int id = 0; id < 255; id++)
        {
            if (GetPlayerById(id) == nullptr)
            {
                *outId = id;
                return true;
            }
        }
        return false;
    }
};

INetworkPlayerManager * CreatePlayerManager(INetworkGroupManager * groupManager, INetworkUserManager * userManager)
{
    return new NetworkPlayerManager(groupManager, userManager);
}

#endif // DISABLE_NETWORK
