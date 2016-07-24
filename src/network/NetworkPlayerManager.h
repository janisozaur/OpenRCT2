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

interface INetworkUserManager;
class NetworkConnection;
class NetworkPlayer;

interface INetworkPlayerList
{
    virtual ~INetworkPlayerList() { }

    virtual bool            IsFull() const abstract;
    virtual uint32          GetCount() const abstract;
    virtual NetworkPlayer * GetPlayerByIndex(uint32 index) const abstract;
    virtual NetworkPlayer * GetPlayerById(uint8 id) const abstract;

    virtual void            UpdatePlayers(NetworkPlayer * players, size_t count) abstract;
};

interface INetworkPlayerManager : public INetworkPlayerList
{
    virtual ~INetworkPlayerManager() { }

    virtual NetworkPlayer * CreatePlayer(const utf8 * name, const char * hash) abstract;
    virtual void            Clear() abstract;
    virtual void            Remove(NetworkPlayer * player) abstract;
};

INetworkPlayerList *    CreatePlayerList(INetworkGroupManager * groupManager);
INetworkPlayerManager * CreatePlayerManager(INetworkGroupManager * groupManager, INetworkUserManager * userManager);
