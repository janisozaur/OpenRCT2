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

#include <string>
#include "../common.h"

class NetworkUser;

interface INetworkUserManager
{
    virtual ~INetworkUserManager() { }

    virtual void Load() abstract;

    /**
     * @brief NetworkUserManager::Save
     * Reads mappings from JSON, updates them in-place and saves JSON.
     *
     * Useful for retaining custom entries in JSON file.
     */
    virtual void Save();

    virtual void UnsetUsersOfGroup(uint8 groupId) abstract;
    virtual void RemoveUser(const std::string &hash) abstract;

    virtual NetworkUser * GetUserByHash(const std::string &hash) abstract;
    virtual const NetworkUser * GetUserByHash(const std::string &hash) const abstract;
    virtual const NetworkUser * GetUserByName(const std::string &name) const abstract;
    virtual NetworkUser * GetOrAddUser(const std::string &hash) abstract;
};
