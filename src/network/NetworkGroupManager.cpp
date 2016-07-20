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

#include <vector>
#include "../core/Console.hpp"
#include "../core/Exception.hpp"
#include "../core/Guard.hpp"
#include "../core/Json.hpp"
#include "../core/Path.hpp"
#include "../core/String.hpp"
#include "../platform/platform.h"
#include "NetworkGroup.h"
#include "NetworkGroupManager.h"

constexpr const utf8 * GROUPS_FILENAME = "groups.json";

class NetworkGroupManager : public INetworkGroupManager
{
private:
    uint8                       _defaultGroupId;
    std::vector<NetworkGroup *> _groups;


public:
    virtual ~NetworkGroupManager() { }

    uint32 GetCount() const override
    {
        return _groups.size();
    }

    NetworkGroup * GetGroupByHash(const char * hash) const override
    {

    }

    NetworkGroup * GetGroupById(uint8 id) const override
    {
        for (auto group : _groups)
        {
            if (group->Id == id)
            {
                return group;
            }
        }
        return nullptr;
    }

    NetworkGroup * GetGroupByIndex(uint32 index) const override
    {
        if (index >= _groups.size()) return nullptr;
        return _groups[index];
    }

    uint8 GetDefaultGroupId() const override
    {
        return _defaultGroupId;
    }

    void SetDefaultGroupId(uint8 groupId) override
    {
        NetworkGroup * group = GetGroupById(groupId);
        Guard::Assert(group != nullptr, GUARD_LINE);

        _defaultGroupId = groupId;
    }

    NetworkGroup * CreateGroup(const utf8 * name) override
    {
        auto group = new NetworkGroup();
        group->SetName(std::string(name));
        _groups.push_back(group);
    }

    void CreateDefaultGroups()
    {
        NetworkGroup * group;

        group = CreateGroup("Admin");
        group->Id = 0;
        group->ActionsAllowed.fill(0xFF);

        group = CreateGroup("Spectator");
        group->Id = 1;
        group->ToggleActionPermission(0);  // Chat

        group = CreateGroup("User");
        group->Id = 2;
        group->ActionsAllowed.fill(0xFF);
        group->ToggleActionPermission(15); // Kick Player
        group->ToggleActionPermission(16); // Modify Groups
        group->ToggleActionPermission(17); // Set Player Group
        group->ToggleActionPermission(18); // Cheat

        SetDefaultGroupId(1);
    }

    void Load() override
    {
        _groups.clear();

        utf8 path[MAX_PATH];
        GetGroupsPath(path, sizeof(path));

        json_t * json = nullptr;
        try
        {
            json = Json::ReadFromFile(path);
        }
        catch (const Exception &e)
        {
            Console::Error::WriteLine("Failed to read %s as JSON. Setting default groups. %s", path, e.GetMsg());
            CreateDefaultGroups();
            return;
        }

        json_t * jsonGroups = json_object_get(json, "groups");
        size_t groupCount = json_array_size(jsonGroups);
        for (size_t i = 0; i < groupCount; i++)
        {
            json_t * jsonGroup = json_array_get(jsonGroups, i);

            NetworkGroup * group = new NetworkGroup(NetworkGroup::FromJson(jsonGroup));
            _groups.push_back(group);
        }

        json_t * jsonDefaultGroup = json_object_get(json, "default_group");
        _defaultGroupId = (uint8)json_integer_value(jsonDefaultGroup);
        if (_defaultGroupId >= _groups.size())
        {
            _defaultGroupId = 0;
        }

        json_decref(json);
    }

    void Save() override
    {
        utf8 path[MAX_PATH];
        GetGroupsPath(path, sizeof(path));

        json_t * jsonGroupsCfg = json_object();
        json_t * jsonGroups = json_array();
        for (auto group : _groups)
        {
            json_array_append_new(jsonGroups, group->ToJson());
        }
        json_object_set_new(jsonGroupsCfg, "default_group", json_integer(_defaultGroupId));
        json_object_set_new(jsonGroupsCfg, "groups", jsonGroups);

        try
        {
            Json::WriteToFile(path, jsonGroupsCfg, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
        }
        catch (Exception ex)
        {
            Console::Error::WriteLine("Unable to save %s: %s", path, ex.GetMsg());
        }

        json_decref(jsonGroupsCfg);
    }

private:
    static void GetGroupsPath(utf8 * buffer, size_t bufferSize)
    {
        platform_get_user_directory(buffer, NULL);
        Path::Append(buffer, bufferSize, GROUPS_FILENAME);
    }
};

INetworkGroupManager * CreateGroupManager()
{
    return new NetworkGroupManager();
}

#endif // DISABLE_NETWORK
