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

#include "../core/Console.hpp"
#include "../core/Guard.hpp"
#include "NetworkChat.h"
#include "NetworkClient.h"
#include "NetworkConnection.h"
#include "NetworkGroupManager.h"
#include "NetworkKey.h"
#include "NetworkPlayerList.h"
#include "NetworkTypes.h"
#include "TcpSocket.h"

extern "C"
{
    #include "../config.h"
    #include "../platform/platform.h"
}

class NetworkClient : public INetworkClient
{
private:
    NETWORK_CLIENT_STATUS   _status;
    SOCKET_STATUS           _lastConnectStatus;
    NetworkConnection *     _serverConnection;
    NetworkKey              _key;

    INetworkChat *          _chat;
    INetworkGroupManager *  _groupManager;
    INetworkPlayerList *    _playerList;

    NetworkServerInfo       _serverInfo;

public:
    NetworkClient()
    {
        _chat = CreateChat();
        _groupManager = CreateGroupManager();
        _playerList = CreatePlayerList(_groupManager, _userManager);
    }

    virtual ~NetworkClient()
    {
        delete _chat;
    }

    INetworkChat * GetNetworkChat() const override
    {
        return _chat;
    }

    INetworkGroupManager * GetGroupManager() const override
    {
        return _groupManager;
    }

    INetworkPlayerList * GetPlayerList() const override
    {
        return _playerList;
    }

    NetworkServerInfo GetServerInfo() const override
    {
        return _serverInfo;
    }

    bool Begin(const char * host, uint16 port) override
    {
        Close();

        Guard::Assert(_serverConnection == nullptr);
        _serverConnection = new NetworkConnection();
        _serverConnection->Socket = CreateTcpSocket();
        _serverConnection->Socket->ConnectAsync(host, port);
        _status = NETWORK_CLIENT_STATUS_CONNECTING;
        _lastConnectStatus = SOCKET_STATUS_CLOSED;

        _chat->StartLogging();

        if (!SetupUserKey())
        {
            return false;
        }
        return true;
    }

    void Close() override
    {

    }

    void Update() override
    {

    }

private:
    bool SetupUserKey()
    {
        utf8 keyPath[MAX_PATH];
        NetworkKey::GetPrivateKeyPath(keyPath, sizeof(keyPath), gConfigNetwork.player_name);
        if (!platform_file_exists(keyPath))
        {
            Console::WriteLine("Generating key... This may take a while");
            Console::WriteLine("Need to collect enough entropy from the system");
            _key.Generate();
            Console::WriteLine("Key generated, saving private bits as %s", keyPath);

            utf8 keysDirectory[MAX_PATH];
            NetworkKey::GetKeysDirectory(keysDirectory, sizeof(keysDirectory));
            if (!platform_ensure_directory_exists(keysDirectory))
            {
                log_error("Unable to create directory %s.", keysDirectory);
                return false;
            }

            SDL_RWops * privkey = SDL_RWFromFile(keyPath, "wb+");
            if (privkey == nullptr)
            {
                log_error("Unable to save private key at %s.", keyPath);
                return false;
            }
            _key.SavePrivate(privkey);
            SDL_RWclose(privkey);

            const std::string hash = _key.PublicKeyHash();
            const utf8 *publicKeyHash = hash.c_str();
            NetworkKey::GetPublicKeyPath(keyPath, sizeof(keyPath), gConfigNetwork.player_name, publicKeyHash);
            Console::WriteLine("Key generated, saving public bits as %s", keyPath);
            SDL_RWops * pubkey = SDL_RWFromFile(keyPath, "wb+");
            if (pubkey == nullptr)
            {
                log_error("Unable to save public key at %s.", keyPath);
                return false;
            }
            _key.SavePublic(pubkey);
            SDL_RWclose(pubkey);
        }
        else
        {
            log_verbose("Loading key from %s", keyPath);
            SDL_RWops * privkey = SDL_RWFromFile(keyPath, "rb");
            if (privkey == nullptr)
            {
                log_error("Unable to read private key from %s.", keyPath);
                return false;
            }

            // LoadPrivate returns validity of loaded key
            bool ok = _key.LoadPrivate(privkey);
            SDL_RWclose(privkey);
            // Don't store private key in memory when it's not in use.
            _key.Unload();
            return ok;
        }
    }
};

INetworkClient * CreateClient()
{
    return new NetworkClient();
}
