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

enum NETWORK_CLIENT_STATUS
{
    NETWORK_CLIENT_STATUS_NONE,
    NETWORK_CLIENT_STATUS_READY,
    NETWORK_CLIENT_STATUS_CONNECTING,
    NETWORK_CLIENT_STATUS_CONNECTED,
};

interface INetworkClient : public INetworkContext
{
    virtual ~INetworkClient() { }

    virtual bool Begin(const char * host, uint16 port) abstract;
};

INetworkClient * CreateClient();
