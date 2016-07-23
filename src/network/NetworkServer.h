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
#include "NetworkContext.h"
#include "NetworkTypes.h"

interface INetworkUserManager;
class NetworkConnection;

interface INetworkServer : public INetworkContext
{
    virtual ~INetworkServer() { }

    virtual INetworkUserManager * GetUserManager() const abstract;

    virtual bool Begin(const char * address, uint16 port) abstract;

    virtual void SetPassword(const utf8 * password) abstract;
    virtual bool HasPassword() const abstract;
    virtual bool CheckPassword(const utf8 * password) const abstract;

    virtual NETWORK_AUTH GetAuthenticationResponse(NetworkConnection * sender,
                                                   const char * gameVersion,
                                                   const char * name,
                                                   const char * password,
                                                   const char * pubkey,
                                                   const char * signature,
                                                   size_t signatureSize) abstract;
    virtual void AcceptPlayer(NetworkConnection * client, const utf8 * name, const char * hash) abstract;
    virtual void KickPlayer(uint8 playerId) abstract;

    virtual void SendToken(NetworkConnection * client) abstract;
    virtual void SendAuthenticationResponse(NetworkConnection * client, NETWORK_AUTH response) abstract;

    virtual void SendErrorMessage(NetworkConnection * client, rct_string_id title, rct_string_id message) abstract;
    virtual void SendGameCommand(uint8 playerId, uint32 * args, uint8 callbackId) abstract;
    virtual void SendGameInformation(NetworkConnection * client);

    virtual void BroadcastMessage(const utf8 * message) abstract;
    virtual void BroadcastMap() abstract;
};

INetworkServer * CreateServer();
