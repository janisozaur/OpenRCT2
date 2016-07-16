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

#pragma once

#include "../core/Memory.hpp"
#include "../core/String.hpp"
#include "network.h"
#include "NetworkConnection.h"
#include "NetworkPlayerList.h"
#include "NetworkServer.h"

extern "C"
{
    #include "../interface/chat.h"
    #include "../openrct2.h"
}

class NetworkServer : public INetworkServer
{
private:
    utf8 * _name;
    utf8 * _description;
    utf8 * _password;
    uint32 _maxPlayers;

    // Provider details
    utf8 * _providerName;
    utf8 * _providerEmail;
    utf8 * _providerWebsite;

    std::list<std::unique_ptr<NetworkConnection>> _clients;

    INetworkPlayerList * _playerList;

public:
    virtual ~NetworkServer() { }

    bool HasPassword() const override
    {
        bool hasPassword = !String::IsNullOrEmpty(_password);
        return hasPassword;
    }

    bool CheckPassword(const utf8 * password) const override
    {
        bool matches = String::Equals(_password, password);
        return matches;
    }

    void AcceptPlayer(NetworkConnection * client, const utf8 * name, const char * hash) override { }

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

    void BroadcastMessage(const utf8 * message) override
    {
        chat_history_add(message);
    }

private:
    void SendPacketToAllClients(NetworkPacket &packet, bool front = false)
    {
        for (auto it = _clients.begin(); it != _clients.end(); it++)
        {
            NetworkConnection * client = (*it).get();
            client->QueuePacket(std::move(NetworkPacket::Duplicate(packet)), front);
        }
    }

    char * GetGameInformationAsJson() const
    {
        char * jsonResult;

        json_t* obj = json_object();
        json_object_set_new(obj, "name", json_string(_name));
        json_object_set_new(obj, "requiresPassword", json_boolean(HasPassword()));
        json_object_set_new(obj, "version", json_string(NETWORK_STREAM_ID));
        json_object_set_new(obj, "players", json_integer(_playerList->GetCount()));
        json_object_set_new(obj, "maxPlayers", json_integer(_maxPlayers));
        json_object_set_new(obj, "description", json_string(_description));
        json_object_set_new(obj, "dedicated", json_boolean(gOpenRCT2Headless));

        // Provider details
        json_t* jsonProvider = json_object();
        json_object_set_new(jsonProvider, "name", json_string(_providerName));
        json_object_set_new(jsonProvider, "email", json_string(_providerEmail));
        json_object_set_new(jsonProvider, "website", json_string(_providerWebsite));
        json_object_set_new(obj, "provider", jsonProvider);

        jsonResult = json_dumps(obj, 0);
        json_decref(obj);

        return jsonResult;
    }
};
