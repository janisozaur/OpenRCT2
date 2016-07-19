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

#include <SDL_platform.h>

#ifdef __WINDOWS__
    // winsock2 must be included before windows.h
    #include <winsock2.h>
#endif

#include "../core/Guard.hpp"
#include "network.h"
#include "Network2.h"
#include "NetworkServer.h"
#include "NetworkClient.h"

#ifndef DISABLE_NETWORK

namespace Network2
{
#ifdef __WINDOWS__
    static bool _wsaInitialised = false;
#endif
    static INetworkContext *    _context = nullptr;
    static NETWORK_MODE         _mode = NETWORK_MODE_NONE;

    INetworkClient * GetClient()
    {
        if (_mode == NETWORK_MODE_CLIENT)
        {
            INetworkClient * client = static_cast<INetworkClient *>(_context);
            return client;
        }
    }

    INetworkContext * GetContext()
    {
        return _context;
    }

    INetworkGroupManager * GetGroupManager()
    {
        return nullptr;
    }

    INetworkPlayerList * GetPlayerList()
    {
        return nullptr;
    }

    INetworkServer * GetServer()
    {
        if (_mode == NETWORK_MODE_SERVER)
        {
            INetworkServer * server = static_cast<INetworkServer *>(_context);
            return server;
        }
    }

    INetworkUserManager * GetUserManager()
    {
        return nullptr;
    }

    NETWORK_MODE GetMode()
    {
        return _mode;
    }

    bool Initialise()
    {
#ifdef __WINDOWS__
        if (!_wsaInitialised)
        {
            log_verbose("Initialising WSA");
            WSADATA wsa_data;
            if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
            {
                log_error("Unable to initialise winsock.");
                return false;
            }
            _wsaInitialised = true;
        }
#endif
    }

    void Dispose()
    {
#ifdef __WINDOWS__
        if (_wsaInitialised)
        {
            WSACleanup();
            _wsaInitialised = false;
        }
#endif
    }

    void Update()
    {
        switch (_mode) {
        case NETWORK_MODE_SERVER:
        {
            INetworkServer * networkServer = Network2::GetServer();
            Guard::Assert(networkServer != nullptr, GUARD_LINE);
            networkServer->Update();
            break;
        }
        case NETWORK_MODE_CLIENT:
        {
            INetworkClient * networkClient = Network2::GetClient();
            Guard::Assert(networkClient != nullptr, GUARD_LINE);
            networkClient->Update();
            break;
        }
        }
    }

    INetworkServer * BeginServer(uint16 port)
    {
        Guard::Assert(_mode == NETWORK_MODE_NONE, GUARD_LINE);

        INetworkServer * server = CreateServer();
        if (!server->Begin(nullptr, port))
        {
            delete server;
            server = nullptr;
        }
        return server;
    }

    INetworkClient * BeginClient(const char * host, uint16 port)
    {
        Guard::Assert(_mode == NETWORK_MODE_NONE, GUARD_LINE);

        INetworkClient * client = CreateClient();
        if (!client->Begin(host, port))
        {
            delete client;
            client = nullptr;
        }
        return client;
    }
}

#endif // DISABLE_NETWORK
