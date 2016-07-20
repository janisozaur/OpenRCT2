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

#include "NetworkConnection.h"
#include "NetworkPacket.h"
#include "NetworkPacketHandler.h"

extern "C"
{
    #include "../localisation/string_ids.h"
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
            Client_Send_GAMEINFO();
            break;

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

#endif // DISABLE_NETWORK
