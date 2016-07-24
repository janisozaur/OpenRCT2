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

#include "../core/String.hpp"
#include "Network2.h"
#include "NetworkClient.h"
#include "NetworkConnection.h"
#include "NetworkGroup.h"
#include "NetworkGroupManager.h"
#include "NetworkPacket.h"
#include "NetworkPacketHandler.h"
#include "NetworkPlayer.h"
#include "NetworkPlayerManager.h"

extern "C"
{
    #include "../interface/window.h"
    #include "../localisation/localisation.h"
    #include "../localisation/string_ids.h"
    #include "../windows/error.h"
}

static rct_string_id GetAuthStringId(NETWORK_AUTH auth)
{
    switch (auth) {
    case NETWORK_AUTH_BADNAME:                  return STR_MULTIPLAYER_BAD_PLAYER_NAME;
    case NETWORK_AUTH_BADVERSION:               return STR_MULTIPLAYER_INCORRECT_SOFTWARE_VERSION;
    case NETWORK_AUTH_BADPASSWORD:              return STR_MULTIPLAYER_BAD_PASSWORD;
    case NETWORK_AUTH_VERIFICATIONFAILURE:      return STR_MULTIPLAYER_VERIFICATION_FAILURE;
    case NETWORK_AUTH_FULL:                     return STR_MULTIPLAYER_SERVER_FULL;
    case NETWORK_AUTH_UNKNOWN_KEY_DISALLOWED:   return STR_MULTIPLAYER_UNKNOWN_KEY_DISALLOWED;
    default:                                    return STR_MULTIPLAYER_INCORRECT_SOFTWARE_VERSION;
    }
}


class ClientPacketHandler : INetworkPacketHandler
{
public:
    void Handle_AUTH(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint8 playerId;
        uint32 authStatus;
        (*packet) >> authStatus >> (uint8&)playerId;

        sender->AuthStatus = (NETWORK_AUTH)authStatus;
        switch (sender->AuthStatus) {
        case NETWORK_AUTH_OK:
        {
            INetworkClient * client = Network2::GetClient();
            client->RequestGameInfo();
            break;
        }
        case NETWORK_AUTH_REQUIREPASSWORD:
            window_network_status_open_password();
            break;

        case NETWORK_AUTH_BADVERSION:
        {
            const char * version = packet->ReadString();
            sender->SetLastDisconnectReason(STR_MULTIPLAYER_INCORRECT_SOFTWARE_VERSION, &version);
            sender->Socket->Disconnect();
            break;
        }
        case NETWORK_AUTH_BADNAME:
        case NETWORK_AUTH_BADPASSWORD:
        case NETWORK_AUTH_FULL:
        case NETWORK_AUTH_UNKNOWN_KEY_DISALLOWED:
        case NETWORK_AUTH_VERIFICATIONFAILURE:
        default:
            rct_string_id errorStringId = GetAuthStringId(sender->AuthStatus);
            sender->SetLastDisconnectReason(errorStringId);
            sender->Socket->Disconnect();
            break;
        }
    }

    void Handle_MAP(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint32 totalDataSize, dataChunkOffset;
        *packet >> totalDataSize >> dataChunkOffset;
        size_t dataChunkSize = (size_t)(packet->size - packet->read);
        if (dataChunkSize > 0)
        {
            void * dataChunk = (void *)packet->Read(dataChunkSize);

            INetworkClient * client = Network2::GetClient();
            client->ReceiveMap(totalDataSize, dataChunkOffset, dataChunk, dataChunkSize);
        }
    }

    void Handle_CHAT(NetworkConnection * sender, NetworkPacket * packet) override
    {
        const utf8 * text = packet->ReadString();
        if (text != nullptr)
        {
            INetworkClient * client = Network2::GetClient();
            client->ReceiveChatMessage(text);
        }
    }

    void Handle_GAMECMD(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint32 tick;
        uint32 args[7];
        uint8 playerid;
        uint8 callback;
        *packet >> tick >> args[0] >> args[1] >> args[2] >> args[3] >> args[4] >> args[5] >> args[6] >> playerid >> callback;

        auto gameCommand = GameCommand(tick, args, playerid, callback);
        INetworkClient * client = Network2::GetClient();
        client->RecieveGameCommand(&gameCommand);
    }

    void Handle_TICK(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint32 tick, srand0;
        *packet >> tick >> srand0;
        
        INetworkClient * client = Network2::GetClient();
        client->RecieveTick(tick, srand0);
    }

    void Handle_PLAYERLIST(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint8 numPlayers;
        *packet >> numPlayers;

        NetworkPlayer * players = new NetworkPlayer[numPlayers];
        for (uint32 i = 0; i < numPlayers; i++)
        {
            players[i].Read(*packet);
        }

        INetworkClient * client = Network2::GetClient();
        INetworkPlayerList * playerList = client->GetPlayerList();
        playerList->UpdatePlayers(players, numPlayers);

        delete[] players;
    }

    void Handle_PING(NetworkConnection * sender, NetworkPacket * packet) override
    {
        INetworkClient * client = Network2::GetClient();
        client->SendPing();
    }

    void Handle_PINGLIST(NetworkConnection * sender, NetworkPacket * packet) override
    {
        INetworkClient * client = Network2::GetClient();
        INetworkPlayerList * playerList = client->GetPlayerList();

        uint8 numPlayers;
        *packet >> numPlayers;
        for (uint32 i = 0; i < numPlayers; i++)
        {
            uint8 id;
            uint16 ping;
            *packet >> id >> ping;

            NetworkPlayer * player = playerList->GetPlayerById(id);
            if (player != nullptr)
            {
                player->ping = ping;
            }
        }

        window_invalidate_by_class(WC_MULTIPLAYER);
        window_invalidate_by_class(WC_PLAYER);
    }

    void Handle_SETDISCONNECTMSG(NetworkConnection * sender, NetworkPacket * packet) override
    {
        const utf8 * disconnectMessage = packet->ReadString();
        if (disconnectMessage != nullptr)
        {
            sender->SetLastDisconnectReason(disconnectMessage);
        }
    }

    void Handle_GAMEINFO(NetworkConnection * sender, NetworkPacket * packet) override
    {
        const char * json = packet->ReadString();
        if (json != nullptr)
        {
            INetworkClient * client = Network2::GetClient();
            client->RecieveServerInfo(json);
        }
    }

    void Handle_SHOWERROR(NetworkConnection * sender, NetworkPacket * packet) override
    {
        rct_string_id title, message;
        *packet >> title >> message;
        window_error_open(title, message);
    }

    void Handle_GROUPLIST(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint8 numGroups;
        uint8 defaultGroupId;
        *packet >> numGroups >> defaultGroupId;

        NetworkGroup * groups = new NetworkGroup[numGroups];
        for (uint32 i = 0; i < numGroups; i++)
        {
            groups[i].Read(*packet);
        }

        INetworkClient * client = Network2::GetClient();
        INetworkGroupManager * groupManager = client->GetGroupManager();
        groupManager->UpdateGroups(groups, numGroups);
        groupManager->SetDefaultGroupId(defaultGroupId);
    }

    void Handle_EVENT(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint16 eventType;
        *packet >> eventType;

        INetworkClient * client = Network2::GetClient();
        switch (eventType) {
        case SERVER_EVENT_PLAYER_JOINED:
        {
            const utf8 * playerName = packet->ReadString();

            char text[256];
            format_string(text, STR_MULTIPLAYER_PLAYER_HAS_JOINED_THE_GAME, &playerName);
            client->ReceiveChatMessage(text);
            break;
        }
        case SERVER_EVENT_PLAYER_DISCONNECTED:
        {
            const utf8 * playerName = packet->ReadString();
            const utf8 * reason = packet->ReadString();
            const utf8 * args[] = { playerName, reason };

            utf8 text[256];
            if (String::IsNullOrEmpty(reason))
            {
                format_string(text, STR_MULTIPLAYER_PLAYER_HAS_DISCONNECTED_NO_REASON, args);
            }
            else
            {
                format_string(text, STR_MULTIPLAYER_PLAYER_HAS_DISCONNECTED_WITH_REASON, args);
            }
            client->ReceiveChatMessage(text);
            break;
        }
        }
    }

    void Handle_TOKEN(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint32 challengeSize;
        *packet >> challengeSize;
        const char * challenge = (const char *)packet->Read(challengeSize);

        INetworkClient * client = Network2::GetClient();
        client->HandleChallenge(challenge, challengeSize);
    }
};

#endif // DISABLE_NETWORK
