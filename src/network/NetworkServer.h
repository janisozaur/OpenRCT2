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

interface INetworkServer
{
    virtual ~INetworkServer() { }

    virtual void Update() abstract;

    virtual bool HasPassword() const abstract;
    virtual bool CheckPassword(const utf8 * password) const abstract;

    virtual void AcceptPlayer(NetworkConnection * client, const utf8 * name, const char * hash) abstract;

    virtual void SendToken(NetworkConnection * client) abstract;
    virtual void SendAuthenticationResponse(NetworkConnection * client, NETWORK_AUTH response) abstract;

    virtual void SendErrorMessage(NetworkConnection * client, rct_string_id title, rct_string_id message) abstract;
    virtual void SendGameCommand(uint8 playerId, uint32 * args, uint8 callbackId) abstract;
    virtual void SendGameInformation(NetworkConnection * client);

    virtual void BroadcastMessage(const utf8 * message) abstract;
};
