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
#include "IPacketHandler.h"
#include "network.h"
#include "NetworkAction.h"
#include "NetworkConnection.h"
#include "NetworkGroup.h"
#include "NetworkGroupManager.h"
#include "NetworkPacket.h"
#include "NetworkPlayerList.h"
#include "NetworkServer.h"

extern "C"
{
    #include "../config.h"
    #include "../interface/window.h"
}

class ServerPacketHandler : IPacketHandler
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

        NETWORK_AUTH response = GetAuthenticationResponse(sender, gameVersion, name, password, pubkey, signature, sigsize);
        INetworkServer * server = Network2::GetServer();
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

                    const char * formatted = FormatChat(sender->Player, text);
                    server->BroadcastMessage(formatted);
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

private:
    NETWORK_AUTH GetAuthenticationResponse(NetworkConnection * sender,
                                           const char * gameVersion,
                                           const char * name,
                                           const char * password,
                                           const char * pubkey,
                                           const char * signature,
                                           size_t signatureSize)
    {
        // Check if there are too many players
        INetworkPlayerList * networkPlayerList = Network2::GetPlayerList();
        if (networkPlayerList->IsFull()) // gConfigNetwork.maxplayers <= player_list.size()
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
        INetworkUserManager * userManager = Network2::GetUserManager();
        if (gConfigNetwork.known_keys_only && userManager->GetUserByHash(hash) == nullptr)
        {
            return NETWORK_AUTH_UNKNOWN_KEY_DISALLOWED;
        }

        // Check password
        INetworkServer * server = Network2::GetServer();
        INetworkGroupManager * groupManager = Network2::GetGroupManager();
        std::string playerHash = sender->Key.PublicKeyHash();
        const NetworkGroup * group = groupManager->GetGroupByHash(playerHash.c_str());
        size_t actionIndex = NetworkActions::FindCommandByPermissionName("PERMISSION_PASSWORDLESS_LOGIN");
        bool passwordless = group->CanPerformAction(actionIndex);
        if (!passwordless)
        {
            if (String::IsNullOrEmpty(password) && server->HasPassword())
            {
                return NETWORK_AUTH_REQUIREPASSWORD;
            }
            else if (server->CheckPassword(password))
            {
                return NETWORK_AUTH_BADPASSWORD;
            }
        }

        // Authentication successful
        sender->AuthStatus = NETWORK_AUTH_OK;
        server->AcceptPlayer(sender, name, playerHash.c_str());
        return NETWORK_AUTH_OK;
    }
};

#endif // DISABLE_NETWORK
