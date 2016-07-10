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

#include "../core/String.hpp"
#include "IPacketHandler.h"
#include "network.h"
#include "NetworkAction.h"
#include "NetworkConnection.h"
#include "NetworkGroup.h"
#include "NetworkPacket.h"

extern "C"
{
    #include "../config.h"
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

        // Check if there are too many players
        if (gConfigNetwork.maxplayers <= player_list.size())
        {
            sender->AuthStatus = NETWORK_AUTH_FULL;
        }

        // Read the packet
        const char * gameVersion = packet->ReadString();
        const char * name = packet->ReadString();
        const char * password = packet->ReadString();
        const char * pubkey = (const char *)packet->ReadString();
        uint32 sigsize;
        (*packet) >> sigsize;
        const char * signature = (const char *)packet->Read(sigsize);

        // Check if the game version matches
        if (!String::Equals(gameVersion, NETWORK_STREAM_ID))
        {
            sender->AuthStatus = NETWORK_AUTH_BADVERSION;
            Server_Send_AUTH(sender);
            return;
        }

        // Check name
        if (String::IsNullOrEmpty(name))
        {
            sender->AuthStatus = NETWORK_AUTH_BADNAME;
            Server_Send_AUTH(sender);
            return;
        }

        // Check if user supplied a key and signature
        if (pubkey == nullptr || signature == nullptr)
        {
            sender->AuthStatus = NETWORK_AUTH_VERIFICATIONFAILURE;
            Server_Send_AUTH(sender);
            return;
        }

        // Verify the user's key
        SDL_RWops * pubkey_rw = SDL_RWFromConstMem(pubkey, strlen(pubkey));
        if (pubkey_rw == nullptr)
        {
            log_verbose("Signature verification failed, invalid data!");
            sender->AuthStatus = NETWORK_AUTH_VERIFICATIONFAILURE;
            Server_Send_AUTH(sender);
            return;
        }

        sender->Key.LoadPublic(pubkey_rw);
        SDL_RWclose(pubkey_rw);
        bool verified = sender->Key.Verify(sender->Challenge.data(),
                                            sender->Challenge.size(),
                                            signature,
                                            sigsize);
        std::string hash = sender->Key.PublicKeyHash();

        if (!verified)
        {
            log_verbose("Signature verification failed!");
            sender->AuthStatus = NETWORK_AUTH_VERIFICATIONFAILURE;
            Server_Send_AUTH(sender);
            return;
        }
        log_verbose("Signature verification ok. Hash %s", hash.c_str());

        // If the server only allows whitelisted keys, check the key is whitelisted
        if (gConfigNetwork.known_keys_only && _userManager.GetUserByHash(hash) == nullptr)
        {
            sender->AuthStatus = NETWORK_AUTH_UNKNOWN_KEY_DISALLOWED;
            Server_Send_AUTH(sender);
            return;
        }

        // Check password
        bool passwordless = false;
        const NetworkGroup * group = GetGroupByID(GetGroupIDByHash(sender->Key.PublicKeyHash()));
        size_t actionIndex = NetworkActions::FindCommandByPermissionName("PERMISSION_PASSWORDLESS_LOGIN");
        passwordless = group->CanPerformAction(actionIndex);
        if (!passwordless)
        {
            if (String::IsNullOrEmpty(password) && Network::password.size() > 0)
            {
                sender->AuthStatus = NETWORK_AUTH_REQUIREPASSWORD;
                Server_Send_AUTH(sender);
                return;
            }
            else if (String::Equals(password, Network::password.c_str()))
            {
                sender->AuthStatus = NETWORK_AUTH_BADPASSWORD;
                Server_Send_AUTH(sender);
                return;
            }
        }

        // Authentication successful
        sender->AuthStatus = NETWORK_AUTH_OK;
        std::string hash = sender->Key.PublicKeyHash();
        Server_Client_Joined(name, hash, sender);
        Server_Send_AUTH(sender);
    }

    void Handle_MAP(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_CHAT(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_GAMECMD(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_TICK(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_PLAYERLIST(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_PING(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_PINGLIST(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_SETDISCONNECTMSG(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_GAMEINFO(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_SHOWERROR(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_GROUPLIST(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_EVENT(NetworkConnection * sender, NetworkPacket * packet) override { }
    void Handle_TOKEN(NetworkConnection * sender, NetworkPacket * packet) override { }
};
