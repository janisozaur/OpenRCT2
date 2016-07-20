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

#include "../common.h"
#include "NetworkTypes.h"

class NetworkConnection;
class NetworkPacket;

interface INetworkPacketHandler
{
    virtual ~INetworkPacketHandler() { }

    virtual void Handle_AUTH                (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_MAP                 (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_CHAT                (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_GAMECMD             (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_TICK                (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_PLAYERLIST          (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_PING                (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_PINGLIST            (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_SETDISCONNECTMSG    (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_GAMEINFO            (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_SHOWERROR           (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_GROUPLIST           (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_EVENT               (NetworkConnection * sender, NetworkPacket * packet) abstract;
    virtual void Handle_TOKEN               (NetworkConnection * sender, NetworkPacket * packet) abstract;

    void HandlePacket(NetworkConnection * sender, NetworkPacket * packet)
    {
        uint32 command;
        (*packet) >> command;

        switch (command) {
        case NETWORK_COMMAND_AUTH:              Handle_AUTH(sender, packet);                break;
        case NETWORK_COMMAND_MAP:               Handle_MAP(sender, packet);                 break;
        case NETWORK_COMMAND_CHAT:              Handle_CHAT(sender, packet);                break;
        case NETWORK_COMMAND_GAMECMD:           Handle_GAMECMD(sender, packet);             break;
        case NETWORK_COMMAND_TICK:              Handle_TICK(sender, packet);                break;
        case NETWORK_COMMAND_PLAYERLIST:        Handle_PLAYERLIST(sender, packet);          break;
        case NETWORK_COMMAND_PING:              Handle_PING(sender, packet);                break;
        case NETWORK_COMMAND_PINGLIST:          Handle_PINGLIST(sender, packet);            break;
        case NETWORK_COMMAND_SETDISCONNECTMSG:  Handle_SETDISCONNECTMSG(sender, packet);    break;
        case NETWORK_COMMAND_GAMEINFO:          Handle_GAMEINFO(sender, packet);            break;
        case NETWORK_COMMAND_SHOWERROR:         Handle_SHOWERROR(sender, packet);           break;
        case NETWORK_COMMAND_GROUPLIST:         Handle_GROUPLIST(sender, packet);           break;
        case NETWORK_COMMAND_EVENT:             Handle_EVENT(sender, packet);               break;
        case NETWORK_COMMAND_TOKEN:             Handle_TOKEN(sender, packet);               break;
        };
    }
};
