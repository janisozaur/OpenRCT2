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

class NetworkGroup;

interface INetworkGroupManager
{
    virtual ~INetworkGroupManager() { }

    virtual uint32          GetCount() const abstract;
    virtual NetworkGroup *  GetGroupById(uint8 id) const abstract;
    virtual NetworkGroup *  GetGroupByIndex(uint32 index) const abstract;
    virtual uint8           GetDefaultGroupId() const abstract;

    virtual void            UpdateGroups(NetworkGroup * groups, size_t numGroups);
    virtual void            SetDefaultGroupId(uint8 groupId) abstract;
    virtual NetworkGroup *  CreateGroup(const utf8 * name) abstract;
    virtual void            RemoveGroup(uint8 groupId) abstract;
    virtual void            Clear() abstract;
    virtual void            Load() abstract;
    virtual void            Save() abstract;
};

INetworkGroupManager * CreateGroupManager();
