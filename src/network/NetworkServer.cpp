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

#include <set>
#include "../core/Console.hpp"
#include "../core/Json.hpp"
#include "../core/Memory.hpp"
#include "../core/String.hpp"
#include "Network2.h"
#include "NetworkAction.h"
#include "NetworkChat.h"
#include "NetworkConnection.h"
#include "NetworkGroup.h"
#include "NetworkGroupManager.h"
#include "NetworkPacketHandler.h"
#include "NetworkPlayer.h"
#include "NetworkPlayerManager.h"
#include "NetworkServer.h"
#include "NetworkServerAdvertiser.h"
#include "NetworkUser.h"
#include "NetworkUserManager.h"

extern "C"
{
    #include "../cheats.h"
    #include "../config.h"
    #include "../game.h"
    #include "../interface/chat.h"
    #include "../localisation/localisation.h"
    #include "../openrct2.h"
}

constexpr uint32 TICK_FREQUENCY_MS = 25;
constexpr uint32 PING_FREQUENCY_MS = 3000;

class NetworkServer : public INetworkServer
{
private:
    uint16 _listeningPort = 0;
    uint16 _listeningAddress = 0;
    bool   _advertise = false;

    uint8   _hostPlayerId = 255;

    std::string _name;
    std::string _description;
    std::string _password;
    uint32      _maxPlayers = 32;

    // Provider details
    std::string _providerName;
    std::string _providerEmail;
    std::string _providerWebsite;

    ITcpSocket * _listeningSocket = nullptr;

    std::list<std::unique_ptr<NetworkConnection>> _clients;
    std::multiset<GameCommand> _gameCommandQueue;

    INetworkServerAdvertiser *  _advertiser     = nullptr;
    INetworkChat *              _chat           = nullptr;
    INetworkGroupManager *      _groupManager   = nullptr;
    INetworkPacketHandler *     _packetHandler  = nullptr;
    INetworkPlayerManager *     _playerManager  = nullptr;
    INetworkUserManager *       _userManager    = nullptr;

    uint32 _lastTickTime = 0;
    uint32 _lastPingTime = 0;

public:
    NetworkServer()
    {
        _chat = CreateChat();
        _groupManager = CreateGroupManager();
        _userManager = CreateUserManager();
        _playerManager = CreatePlayerManager(_groupManager, _userManager);
    }

    virtual ~NetworkServer()
    {
        delete _playerManager;
    }

    bool Begin(const char * address, uint16 port) override
    {
        Close();

        Guard::Assert(Network2::GetMode() == NETWORK_MODE_NONE, GUARD_LINE);
        Network2::SetMode(NETWORK_MODE_SERVER);

        _userManager->Load();

        log_verbose("Begin listening for clients");

        Guard::Assert(_listeningSocket == nullptr, GUARD_LINE);
        _listeningSocket = CreateTcpSocket();
        try
        {
            _listeningSocket->Listen(address, port);
        }
        catch (Exception ex)
        {
            Console::Error::WriteLine(ex.GetMsg());
            Close();
            return false;
        }

        // Copy details from user config
        _name = std::string(gConfigNetwork.server_name);
        _description = std::string(gConfigNetwork.server_description);
        _providerName = std::string(gConfigNetwork.provider_name);
        _providerEmail = std::string(gConfigNetwork.provider_email);
        _providerWebsite = std::string(gConfigNetwork.provider_website);

        cheats_reset();
        _groupManager->Load();
        _chat->StartLogging();

        NetworkPlayer * hostPlayer = _playerManager->CreatePlayer(gConfigNetwork.player_name, "");
        hostPlayer->flags |= NETWORK_PLAYER_FLAG_ISSERVER;
        hostPlayer->group = 0;
        _hostPlayerId = hostPlayer->id;

        Console::WriteLine("Ready for clients...");
        _chat->ShowChatHelp();

        _listeningPort = port;

        if (_advertise)
        {
            Guard::Assert(_advertiser == nullptr, GUARD_LINE);
            _advertiser = CreateServerAdvertiser(port);
        }

        return true;
    }

    void Close() override
    {
        delete _advertiser;
        _advertiser = nullptr;

        // Stop listening for connections
        delete _listeningSocket;
        _listeningSocket = nullptr;

        // mode = NETWORK_MODE_NONE;
        _clients.clear();

        _gameCommandQueue.clear();
        _playerManager->Clear();
        _groupManager->Clear();
        _chat->StopLogging();
        gfx_invalidate_screen();

        Network2::SetMode(NETWORK_MODE_NONE);
    }

    void Update() override
    {
        ProcessClients();
        PingClients();
        Advertise();
        AcceptNewClients();
    }

    void SendChatMessage(const utf8 * text) override
    {
        NetworkPlayer * player = _playerManager->GetPlayerById(_hostPlayerId);
        _chat->ShowMessage(player, text);

        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_CHAT;
        packet->WriteString(text);
        SendPacketToAllClients(*packet);
    }

    NetworkServerInfo GetServerInfo() const override
    {
        NetworkServerInfo info;
        info.Name = _name.c_str();
        info.Description = _description.c_str();
        info.Provider.Name = _providerName.c_str();
        info.Provider.Email = _providerEmail.c_str();
        info.Provider.Website = _providerWebsite.c_str();
        return info;
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
        return _playerManager;
    }

    INetworkUserManager * GetUserManager() const override
    {
        return _userManager;
    }

    uint8 GetPlayerId() const override
    {
        return _hostPlayerId;
    }

    void SetPassword(const utf8 * password) override
    {
        _password = std::string(password);
    }

    bool HasPassword() const override
    {
        bool hasPassword = !_password.empty();
        return hasPassword;
    }

    bool CheckPassword(const utf8 * password) const override
    {
        bool matches = String::Equals(_password.c_str(), password);
        return matches;
    }

    NETWORK_AUTH GetAuthenticationResponse(NetworkConnection * sender,
                                           const char * gameVersion,
                                           const char * name,
                                           const char * password,
                                           const char * pubkey,
                                           const char * signature,
                                           size_t signatureSize)
    {
        // Check if there are too many players
        if (_playerManager->IsFull()) // gConfigNetwork.maxplayers <= player_list.size()
        {
            return NETWORK_AUTH_FULL;
        }

        // Check if the game version matches
        if (!String::Equals(gameVersion, NETWORK_STREAM_ID))
        {
            return NETWORK_AUTH_BADVERSION;
        }

        // Check name
        if (String::IsNullOrEmpty(name))
        {
            return NETWORK_AUTH_BADNAME;
        }

        // Check if user supplied a key and signature
        if (pubkey == nullptr || signature == nullptr)
        {
            return NETWORK_AUTH_VERIFICATIONFAILURE;
        }

        // Verify the user's key
        SDL_RWops * pubkey_rw = SDL_RWFromConstMem(pubkey, strlen(pubkey));
        if (pubkey_rw == nullptr)
        {
            log_verbose("Signature verification failed, invalid data!");
            return NETWORK_AUTH_VERIFICATIONFAILURE;
        }

        sender->Key.LoadPublic(pubkey_rw);
        SDL_RWclose(pubkey_rw);
        bool verified = sender->Key.Verify(sender->Challenge.data(),
                                           sender->Challenge.size(),
                                           signature,
                                           signatureSize);
        std::string hash = sender->Key.PublicKeyHash();

        if (!verified)
        {
            log_verbose("Signature verification failed!");
            return NETWORK_AUTH_VERIFICATIONFAILURE;
        }
        log_verbose("Signature verification ok. Hash %s", hash.c_str());

        // If the server only allows whitelisted keys, check the key is whitelisted
        if (gConfigNetwork.known_keys_only && _userManager->GetUserByHash(hash) == nullptr)
        {
            return NETWORK_AUTH_UNKNOWN_KEY_DISALLOWED;
        }

        // Check password
        std::string playerHash = sender->Key.PublicKeyHash();
        NetworkUser * playerUser = _userManager->GetUserByHash(playerHash);
        uint8 playerGroupId = playerUser->GroupId.GetValueOrDefault(_groupManager->GetDefaultGroupId());
        const NetworkGroup * group = _groupManager->GetGroupById(playerGroupId);
        size_t actionIndex = NetworkActions::FindCommandByPermissionName("PERMISSION_PASSWORDLESS_LOGIN");
        bool passwordless = group->CanPerformAction(actionIndex);
        if (!passwordless)
        {
            if (String::IsNullOrEmpty(password) && HasPassword())
            {
                return NETWORK_AUTH_REQUIREPASSWORD;
            }
            else if (CheckPassword(password))
            {
                return NETWORK_AUTH_BADPASSWORD;
            }
        }

        // Authentication successful
        sender->AuthStatus = NETWORK_AUTH_OK;
        AcceptPlayer(sender, name, playerHash.c_str());
        return NETWORK_AUTH_OK;
    }

    void AcceptPlayer(NetworkConnection * client, const utf8 * name, const char * hash) override
    {
        NetworkPlayer * player = _playerManager->CreatePlayer(name, hash);
        if (player != nullptr)
        {
            client->Player = player;
        }
    }

    void KickPlayer(uint8 playerId) override
    {
        for (auto it = _clients.begin(); it != _clients.end(); it++)
        {
            NetworkConnection * client = it->get();
            if (client->Player->id == playerId)
            {
                // Disconnect the client gracefully
                client->SetLastDisconnectReason(STR_MULTIPLAYER_KICKED);
                char str_disconnect_msg[256];
                format_string(str_disconnect_msg, STR_MULTIPLAYER_KICKED_REASON, NULL);

                std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
                *packet << (uint32)NETWORK_COMMAND_SETDISCONNECTMSG;
                packet->WriteString(str_disconnect_msg);
                client->QueuePacket(std::move(packet));

                client->Socket->Disconnect();
                client->SendQueuedPackets();
                break;
            }
        }
    }

    void SendToken(NetworkConnection * client) override
    {
        uint32 challengeDataSize = (uint32)client->Challenge.size();
        const uint8 * challengeData = client->Challenge.data();

        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_TOKEN << challengeDataSize;
        packet->Write(challengeData, challengeDataSize);
        client->QueuePacket(std::move(packet));
    }

    void SendAuthenticationResponse(NetworkConnection * client, NETWORK_AUTH response) override
    {
        uint8 newPlayerId = 0;
        if (client->Player != nullptr)
        {
            newPlayerId = client->Player->id;
        }

        // Send the response packet
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_AUTH << (uint32)client->AuthStatus << (uint8)newPlayerId;
        if (client->AuthStatus == NETWORK_AUTH_BADVERSION)
        {
            packet->WriteString(NETWORK_STREAM_ID);
        }
        client->QueuePacket(std::move(packet));

        if (client->AuthStatus != NETWORK_AUTH_OK &&
            client->AuthStatus != NETWORK_AUTH_REQUIREPASSWORD)
        {
            // The client failed the authentication, so send the remaining packets
            // and then terminate the client connection
            client->SendQueuedPackets();
            client->Socket->Disconnect();
        }
    }

    void SendErrorMessage(NetworkConnection * client, rct_string_id title, rct_string_id message) override
    {
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_SHOWERROR << title << message;
        client->QueuePacket(std::move(packet));
    }

    void SendGameCommand(uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, uint32 esi, uint32 edi, uint32 ebp, uint8 callbackId) override
    {
        uint32 args[] = { eax, ebx, ecx, edx, esi, edi, ebp };
        SendGameCommand(_hostPlayerId, args, callbackId);
    }

    void SendGameCommand(uint8 playerId, uint32 * args, uint8 callbackId) override
    {
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_GAMECMD
                << (uint32)gCurrentTicks
                << args[0]
                << (args[1] | GAME_COMMAND_FLAG_NETWORKED)
                << args[2]
                << args[3]
                << args[4]
                << args[5]
                << args[6]
                << playerId
                << callbackId;
        SendPacketToAllClients(*packet);
    }

    void SendGameInformation(NetworkConnection * client)
    {
        char * gameInfoJson = GetGameInformationAsJson();

        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_GAMEINFO;
        packet->WriteString(gameInfoJson);
        client->QueuePacket(std::move(packet));

        Memory::Free(gameInfoJson);
    }

    void BroadcastMessage(uint8 playerId, const utf8 * message) override
    {
        NetworkPlayer * player = _playerManager->GetPlayerById(playerId);
        if (player != nullptr)
        {
            _chat->ShowMessage(player, message);

            std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
            *packet << (uint32)NETWORK_COMMAND_CHAT;
            packet->WriteString(message);
            SendPacketToAllClients(*packet);
        }
    }

    void BroadcastMap() override
    {

    }

private:
    void Advertise()
    {
        if (_advertiser != nullptr)
        {
            _advertiser->Update();
        }
    }

    void SendPacketToAllClients(NetworkPacket &packet, bool front = false)
    {
        for (auto it = _clients.begin(); it != _clients.end(); it++)
        {
            NetworkConnection * client = (*it).get();
            client->QueuePacket(std::move(NetworkPacket::Duplicate(packet)), front);
        }
    }

    void ProcessClients()
    {
        auto it = _clients.begin();
        while (it != _clients.end())
        {
            NetworkConnection &connection = *(*it);
            if (!ProcessConnection(&connection))
            {
                RemoveClient((*it));
                it = _clients.begin();
            }
            else
            {
                it++;
            }
        }
    }

    bool ProcessConnection(NetworkConnection * connection)
    {
        int packetStatus;
        do
        {
            packetStatus = connection->ReadPacket();
            switch (packetStatus) {
            case NETWORK_READPACKET_DISCONNECTED:
                // closed connection or network error
                if (!connection->GetLastDisconnectReason())
                {
                    connection->SetLastDisconnectReason(STR_MULTIPLAYER_CONNECTION_CLOSED);
                }
                return false;
            case NETWORK_READPACKET_SUCCESS:
                // done reading in packet
                ProcessPacket(connection, &connection->InboundPacket);
                if (connection->Socket == nullptr)
                {
                    return false;
                }
                break;
            case NETWORK_READPACKET_MORE_DATA:
                // more data required to be read
                break;
            case NETWORK_READPACKET_NO_DATA:
                // could not read anything from socket
                break;
            }
        }
        while (packetStatus == NETWORK_READPACKET_MORE_DATA ||
               packetStatus == NETWORK_READPACKET_SUCCESS);

        connection->SendQueuedPackets();
        if (!connection->ReceivedPacketRecently())
        {
            if (!connection->GetLastDisconnectReason())
            {
                connection->SetLastDisconnectReason(STR_MULTIPLAYER_NO_DATA);
            }
            return false;
        }
        return true;
    }

    void ProcessPacket(NetworkConnection * sender, NetworkPacket * packet)
    {
        bool handlePacket = true;
        if (sender->AuthStatus != NETWORK_AUTH_OK)
        {
            NETWORK_COMMAND command = (NETWORK_COMMAND)packet->GetCommand();
            if (CommandRequiresAuthentication(command))
            {
                // Ignore as player must be authentication first before sending this
                handlePacket = false;
            }
        }

        if (handlePacket)
        {
            _packetHandler->HandlePacket(sender, packet);
        }

        packet->Clear();
    }

    void PingClients()
    {
        if (SDL_TICKS_PASSED(SDL_GetTicks(), _lastTickTime + TICK_FREQUENCY_MS))
        {
            SendTickToClients();
        }
        if (SDL_TICKS_PASSED(SDL_GetTicks(), _lastPingTime + PING_FREQUENCY_MS))
        {
            SendPingToClients();
            SendPingListToClients();
        }
    }

    void AcceptNewClients()
    {
        ITcpSocket * tcpSocket = _listeningSocket->Accept();
        if (tcpSocket != nullptr)
        {
            AddClient(tcpSocket);
        }
    }

    void AddClient(ITcpSocket * socket)
    {
        auto client = std::unique_ptr<NetworkConnection>(new NetworkConnection());  // change to make_unique in c++14
        client->Socket = socket;
        _clients.push_back(std::move(client));
    }

    void RemoveClient(std::unique_ptr<NetworkConnection> &client)
    {
        NetworkPlayer * player = client->Player;
        if (player != nullptr)
        {
            _playerManager->Remove(player);

            const utf8 * playerName = player->name.c_str();
            const utf8 * disconnectReason = client->GetLastDisconnectReason();

            rct_string_id stringId = String::IsNullOrEmpty(disconnectReason) ?
                STR_MULTIPLAYER_PLAYER_HAS_DISCONNECTED_NO_REASON :
                STR_MULTIPLAYER_PLAYER_HAS_DISCONNECTED_WITH_REASON;
            const utf8 * stringArgs[] = { playerName, disconnectReason };

            char text[256];
            format_string(text, stringId, stringArgs);
            chat_history_add(text);

            // Give all the clients the updated player list
            SendEventPlayerDisconnected(playerName, disconnectReason);
            SendPlayerListToClients();
        }

        _clients.remove(client);
    }

    char * GetGameInformationAsJson() const
    {
        char * jsonResult;

        json_t* obj = json_object();
        json_object_set_new(obj, "name", json_string(_name.c_str()));
        json_object_set_new(obj, "requiresPassword", json_boolean(HasPassword()));
        json_object_set_new(obj, "version", json_string(NETWORK_STREAM_ID));
        json_object_set_new(obj, "players", json_integer(_playerManager->GetCount()));
        json_object_set_new(obj, "maxPlayers", json_integer(_maxPlayers));
        json_object_set_new(obj, "description", json_string(_description.c_str()));
        json_object_set_new(obj, "dedicated", json_boolean(gOpenRCT2Headless));

        // Provider details
        json_t* jsonProvider = json_object();
        json_object_set_new(jsonProvider, "name", json_string(_providerName.c_str()));
        json_object_set_new(jsonProvider, "email", json_string(_providerEmail.c_str()));
        json_object_set_new(jsonProvider, "website", json_string(_providerWebsite.c_str()));
        json_object_set_new(obj, "provider", jsonProvider);

        jsonResult = json_dumps(obj, 0);
        json_decref(obj);

        return jsonResult;
    }

    void SendTickToClients()
    {
        _lastTickTime = SDL_GetTicks();
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_TICK << (uint32)gCurrentTicks << (uint32)gScenarioSrand0;
        SendPacketToAllClients(*packet);
    }

    void SendPingToClients()
    {
        _lastPingTime = SDL_GetTicks();
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_PING;
        for (auto it = _clients.begin(); it != _clients.end(); it++)
        {
            (*it)->PingTime = SDL_GetTicks();
        }
        SendPacketToAllClients(*packet, true);
    }

    void SendPingListToClients()
    {
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_PINGLIST << (uint8)_playerManager->GetCount();
        for (uint32 i = 0; i < _playerManager->GetCount(); i++)
        {
            NetworkPlayer * player = _playerManager->GetPlayerByIndex(i);
            *packet << player->id << player->ping;
        }
        SendPacketToAllClients(*packet);
    }

    void SendEventPlayerDisconnected(const utf8 * playerName, const utf8 * reason)
    {
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_EVENT;
        *packet << (uint16)SERVER_EVENT_PLAYER_DISCONNECTED;
        packet->WriteString(playerName);
        packet->WriteString(reason);
        SendPacketToAllClients(*packet);
    }

    void SendPlayerListToClients()
    {
        std::unique_ptr<NetworkPacket> packet = std::move(NetworkPacket::Allocate());
        *packet << (uint32)NETWORK_COMMAND_PLAYERLIST << (uint8)_playerManager->GetCount();
        for (uint32 i = 0; i < _playerManager->GetCount(); i++)
        {
            NetworkPlayer * player = _playerManager->GetPlayerByIndex(i);
            player->Write(*packet);
        }
        SendPacketToAllClients(*packet);
    }

    static bool CommandRequiresAuthentication(NETWORK_COMMAND command)
    {
        switch (command) {
        case NETWORK_COMMAND_PING:
        case NETWORK_COMMAND_AUTH:
        case NETWORK_COMMAND_TOKEN:
        case NETWORK_COMMAND_GAMEINFO:
            return false;
        default:
            return true;
        }
    }
};

INetworkServer * CreateServer()
{
    return new NetworkServer();
}

#endif // DISABLE_NETWORK
