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

class NetworkConnection;
class NetworkPacket;

interface IPacketHandler
{
    virtual ~IPacketHandler() { }

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
};
