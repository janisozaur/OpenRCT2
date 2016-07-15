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

#include "NetworkUser.h"

NetworkUser * NetworkUser::FromJson(json_t * json)
{
    const char * hash = json_string_value(json_object_get(json, "hash"));
    const char * name = json_string_value(json_object_get(json, "name"));
    const json_t * jsonGroupId = json_object_get(json, "groupId");

    NetworkUser * user = nullptr;
    if (hash != nullptr && name != nullptr)
    {
        user = new NetworkUser();
        user->Hash = std::string(hash);
        user->Name = std::string(name);
        if (!json_is_null(jsonGroupId))
        {
            user->GroupId = (uint8)json_integer_value(jsonGroupId);
        }
        user->Remove = false;
        return user;
    }
    return user;
}

json_t * NetworkUser::ToJson() const
{
    return ToJson(json_object());
}

json_t * NetworkUser::ToJson(json_t * json) const
{
    json_object_set_new(json, "hash", json_string(Hash.c_str()));
    json_object_set_new(json, "name", json_string(Name.c_str()));

    json_t * jsonGroupId;
    if (GroupId.HasValue())
    {
        jsonGroupId = json_integer(GroupId.GetValue());
    }
    else
    {
        jsonGroupId = json_null();
    }
    json_object_set_new(json, "groupId", jsonGroupId);

    return json;
}

#endif
