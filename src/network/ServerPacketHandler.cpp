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

#include "../core/Math.hpp"
#include "../core/String.hpp"
#include "Network2.h"
#include "NetworkAction.h"
#include "NetworkConnection.h"
#include "NetworkGroup.h"
#include "NetworkGroupManager.h"
#include "NetworkPacket.h"
#include "NetworkPacketHandler.h"
#include "NetworkPlayer.h"
#include "NetworkPlayerManager.h"
#include "NetworkServer.h"

extern "C"
{
    #include "../config.h"
    #include "../game.h"
    #include "../interface/window.h"
    #include "../localisation/string_ids.h"
}

class ServerPacketHandler : public INetworkPacketHandler
{
public:
    void Handle_AUTH(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // Check if the user is already authenticated
        if (sender->AuthStatus == NETWORK_AUTH_OK)
        {
            return;
        }

        // Read the packet
        const char * gameVersion = packet->ReadString();
        const char * name = packet->ReadString();
        const char * password = packet->ReadString();
        const char * pubkey = (const char *)packet->ReadString();
        uint32 sigsize;
        (*packet) >> sigsize;
        const char * signature = (const char *)packet->Read(sigsize);

        INetworkServer * server = Network2::GetServer();
        NETWORK_AUTH response =
            server->GetAuthenticationResponse(sender, gameVersion, name, password, pubkey, signature, sigsize);
        sender->AuthStatus = response;
        server->SendAuthenticationResponse(sender, response);
    }

    void Handle_MAP(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_CHAT(NetworkConnection * sender, NetworkPacket * packet) override
    {
        if (sender->Player != nullptr)
        {
            INetworkGroupManager * groupManager = Network2::GetGroupManager();
            NetworkGroup * group = groupManager->GetGroupById(sender->Player->group);
            if (group != nullptr && group->CanPerformCommand(-1))
            {
                const char * text = packet->ReadString();
                if (!String::IsNullOrEmpty(text))
                {
                    INetworkServer * server = Network2::GetServer();
                    server->BroadcastMessage(sender->Player->id, text);
                }
            }
        }
    }

    void Handle_GAMECMD(NetworkConnection * sender, NetworkPacket * packet) override
    {
        if (sender->Player == nullptr) return;

        uint32 tick;
        uint32 args[7];
        uint8 callback;
        *packet >> tick >> args[0] >> args[1] >> args[2] >> args[3] >> args[4] >> args[5] >> args[6] >> callback;

        int commandCommand = args[4];

        // Check if player's group permission allows command to run
        INetworkServer * server = Network2::GetServer();
        INetworkGroupManager * groupManager = Network2::GetGroupManager();
        NetworkGroup * group = groupManager->GetGroupById(sender->Player->group);
        if (group == nullptr || !group->CanPerformCommand(commandCommand))
        {
            server->SendErrorMessage(sender, STR_CANT_DO_THIS, STR_PERMISSION_DENIED);
            return;
        }

        // In case someone modifies the code / memory to enable cluster build,
        // require a small delay in between placing scenery to provide some security, as 
        // cluster mode is a for loop that runs the place_scenery code multiple times.
        if (commandCommand == GAME_COMMAND_PLACE_SCENERY)
        {
            // Tick count is different by time last_action_time is set, keep same value.
            uint32 ticks = SDL_GetTicks();
            if ((ticks - sender->Player->last_action_time) < 20)
            {
                if (!(group->CanPerformCommand(-2)))
                {
                    server->SendErrorMessage(sender, STR_CANT_DO_THIS, STR_CANT_DO_THIS);
                    return;
                }
            }
        }

        // Don't let clients send pause or quit
        if (commandCommand == GAME_COMMAND_TOGGLE_PAUSE ||
            commandCommand == GAME_COMMAND_LOAD_OR_QUIT)
        {
            return;
        }

        // Set this to reference inside of game command functions
        game_command_playerid = sender->Player->id;

        // Run game command, and if it is successful send to clients
        money32 cost = game_do_command(args[0], args[1] | GAME_COMMAND_FLAG_NETWORKED, args[2], args[3], args[4], args[5], args[6]);
        if (cost == MONEY32_UNDEFINED)
        {
            return;
        }

        sender->Player->last_action = NetworkActions::FindCommand(commandCommand);
        sender->Player->last_action_time = SDL_GetTicks();
        sender->Player->AddMoneySpent(cost);
        server->SendGameCommand(sender->Player->id, args, callback);
    }

    void Handle_TICK(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_PLAYERLIST(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_PING(NetworkConnection * sender, NetworkPacket * packet) override
    {
        sint32 ping = (sint32)(SDL_GetTicks() - sender->PingTime);
        ping = Math::Max(ping, 0);

        if (sender->Player != nullptr)
        {
            sender->Player->ping = ping;
            window_invalidate_by_number(WC_PLAYER, sender->Player->id);
        }
    }

    void Handle_PINGLIST(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_SETDISCONNECTMSG(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_GAMEINFO(NetworkConnection * sender, NetworkPacket * packet) override
    {
        INetworkServer * server = Network2::GetServer();
        server->SendGameInformation(sender);
    }

    void Handle_SHOWERROR(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_GROUPLIST(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_EVENT(NetworkConnection * sender, NetworkPacket * packet) override
    {
        // No action required
    }

    void Handle_TOKEN(NetworkConnection * sender, NetworkPacket * packet) override
    {
        uint8 tokenSize = 10 + (rand() & 0x7F);
        sender->Challenge.resize(tokenSize);
        for (int i = 0; i < tokenSize; i++)
        {
            sender->Challenge[i] = (uint8)(rand() & 0xFF);
        }

        INetworkServer * server = Network2::GetServer();
        server->SendToken(sender);
    }
};

#endif // DISABLE_NETWORK
