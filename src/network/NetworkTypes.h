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

#include <SDL_endian.h>
#include <SDL_platform.h>

#ifndef DISABLE_NETWORK


#include "../common.h"
#endif

enum NETWORK_AUTH
{
    NETWORK_AUTH_NONE,
    NETWORK_AUTH_REQUESTED,
    NETWORK_AUTH_OK,
    NETWORK_AUTH_BADVERSION,
    NETWORK_AUTH_BADNAME,
    NETWORK_AUTH_BADPASSWORD,
    NETWORK_AUTH_VERIFICATIONFAILURE,
    NETWORK_AUTH_FULL,
    NETWORK_AUTH_REQUIREPASSWORD,
    NETWORK_AUTH_VERIFIED,
    NETWORK_AUTH_UNKNOWN_KEY_DISALLOWED,
};

enum NETWORK_COMMAND
{
    NETWORK_COMMAND_AUTH,
    NETWORK_COMMAND_MAP,
    NETWORK_COMMAND_CHAT,
    NETWORK_COMMAND_GAMECMD,
    NETWORK_COMMAND_TICK,
    NETWORK_COMMAND_PLAYERLIST,
    NETWORK_COMMAND_PING,
    NETWORK_COMMAND_PINGLIST,
    NETWORK_COMMAND_SETDISCONNECTMSG,
    NETWORK_COMMAND_GAMEINFO,
    NETWORK_COMMAND_SHOWERROR,
    NETWORK_COMMAND_GROUPLIST,
    NETWORK_COMMAND_EVENT,
    NETWORK_COMMAND_TOKEN,
    NETWORK_COMMAND_MAX,
    NETWORK_COMMAND_INVALID = -1
};

enum SERVER_EVENT_PLAYER
{
    SERVER_EVENT_PLAYER_JOINED,
    SERVER_EVENT_PLAYER_DISCONNECTED,
};

enum NETWORK_CLIENT_STATUS
{
    NETWORK_CLIENT_STATUS_NONE,
    NETWORK_CLIENT_STATUS_READY,
    NETWORK_CLIENT_STATUS_CONNECTING,
    NETWORK_CLIENT_STATUS_CONNECTED,
};

enum NETWORK_MODE
{
    NETWORK_MODE_NONE,
    NETWORK_MODE_CLIENT,
    NETWORK_MODE_SERVER
};

enum NETWORK_PLAYER_FLAG
{
    NETWORK_PLAYER_FLAG_ISSERVER = 1 << 0,
};

typedef struct NetworkServerProviderInfo
{
    const utf8 * Name;
    const utf8 * Email;
    const utf8 * Website;
} NetworkServerProviderInfo;

typedef struct NetworkServerInfo
{
    const utf8 * Name;
    const utf8 * Description;
    NetworkServerProviderInfo Provider;
} NetworkServerInfo;

#ifdef __cplusplus

struct GameCommand
{
    GameCommand(uint32 t, uint32* args, uint8 p, uint8 cb)
    {
        tick = t;
        eax = args[0];
        ebx = args[1];
        ecx = args[2];
        edx = args[3];
        esi = args[4];
        edi = args[5];
        ebp = args[6];
        playerid = p;
        callback = cb;
    }

    uint32 tick;
    uint32 eax, ebx, ecx, edx, esi, edi, ebp;
    uint8 playerid;
    uint8 callback;

    bool operator <(const GameCommand &comp) const
    {
        return tick < comp.tick;
    }
};

template <size_t size>
struct ByteSwapT { };

template <>
struct ByteSwapT<1>
{
    static uint8 SwapBE(uint8 value)
    {
        return value;
    }
};

template <>
struct ByteSwapT<2>
{
    static uint16 SwapBE(uint16 value)
    {
        return SDL_SwapBE16(value);
    }
};

template <>
struct ByteSwapT<4>
{
    static uint32 SwapBE(uint32 value)
    {
        return SDL_SwapBE32(value);
    }
};

template <typename T>
T ByteSwapBE(const T& value)
{
    return ByteSwapT<sizeof(T)>::SwapBE(value);
}

#endif
