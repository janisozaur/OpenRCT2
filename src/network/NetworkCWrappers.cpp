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

#include "../core/Math.hpp"
#include "../core/String.hpp"
#include "../core/Util.hpp"
#include "network.h"
#include "Network2.h"
#include "NetworkAction.h"
#include "NetworkClient.h"
#include "NetworkGroup.h"
#include "NetworkGroupManager.h"
#include "NetworkPlayer.h"
#include "NetworkPlayerManager.h"
#include "NetworkServer.h"
#include "NetworkUser.h"
#include "NetworkUserManager.h"

extern "C"
{
    #include "../game.h"
    #include "../interface/window.h"
    #include "../localisation/localisation.h"
}

extern "C"
{

int network_init()
{
    return Network2::Initialise();
}

void network_update()
{
    Network2::Update();
}

void network_close()
{
    Network2::Dispose();
}

int network_begin_server(int port)
{
    INetworkServer * server = Network2::BeginServer(port);
    return server != nullptr;
}

int network_begin_client(const char * host, int port)
{
    INetworkClient * client = Network2::BeginClient(host, port);
    return client != nullptr;
}

void network_shutdown_client()
{
    INetworkClient * client = Network2::GetClient();
    if (client != nullptr)
    {
        client->Close();

        // Double check if we can just delete it here
        delete client;
    }
}

int network_get_mode()
{
    return Network2::GetMode();
}

int network_get_status()
{
    INetworkClient * client = Network2::GetClient();
    return client->GetConnectionStatus();
}

int network_get_authstatus()
{
    INetworkClient * client = Network2::GetClient();
    return client->GetAuthStatus();
}

uint32 network_get_server_tick()
{
    INetworkClient * client = Network2::GetClient();
    return client->GetServerTick();
}

uint8 network_get_current_player_id()
{
    INetworkContext * context = Network2::GetContext();
    if (context == nullptr)
    {
        return -1;
    }
    return context->GetPlayerId();
}

int network_get_num_players()
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    return playerList->GetCount();
}

const char * network_get_player_name(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->name.c_str();
}

uint32 network_get_player_flags(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->flags;
}

int network_get_player_ping(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->ping;
}

int network_get_player_id(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->id;
}

money32 network_get_player_money_spent(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->money_spent;
}

void network_add_player_money_spent(unsigned int index, money32 cost)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    player->AddMoneySpent(cost);
}

int network_get_player_last_action(unsigned int index, int time)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);

    if (time && SDL_TICKS_PASSED(SDL_GetTicks(), player->last_action_time + time))
    {
        return -999;
    }
    return player->last_action;
}

void network_set_player_last_action(unsigned int index, int command)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    player->last_action = NetworkActions::FindCommand(command);
    player->last_action_time = SDL_GetTicks();
}

rct_xyz16 network_get_player_last_action_coord(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->last_action_coord;
}

void network_set_player_last_action_coord(unsigned int index, rct_xyz16 coord)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    player->last_action_coord = coord;
}

unsigned int network_get_player_commands_ran(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->commands_ran;
}

int network_get_player_index(uint8 id)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();

    for (uint32 i = 0; i < playerList->GetCount(); i++)
    {
        NetworkPlayer * player = playerList->GetPlayerByIndex(i);
        if (player->id == id)
        {
            return (int)i;
        }
    }
    return -1;
}

uint8 network_get_player_group(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerByIndex(index);
    return player->group;
}

int network_get_group_index(uint8 id)
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();

    for (uint32 i = 0; i < groupManager->GetCount(); i++)
    {
        NetworkGroup * group = groupManager->GetGroupById(i);
        if (group->Id == id)
        {
            return (int)i;
        }
    }
    return -1;
}

uint8 network_get_group_id(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();
    NetworkGroup * group = groupManager->GetGroupByIndex(index);
    return group->Id;
}

int network_get_num_groups()
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();
    return groupManager->GetCount();
}

const char * network_get_group_name(unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();
    NetworkGroup * group = groupManager->GetGroupByIndex(index);
    return group->GetName().c_str();
}

uint8 network_get_default_group()
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();
    return groupManager->GetDefaultGroupId();
}

int network_get_num_actions()
{
    return NetworkActions::Actions.size();
}

rct_string_id network_get_action_name_string_id(unsigned int index)
{
    return NetworkActions::Actions[index].Name;
}

int network_can_perform_action(unsigned int groupindex, unsigned int index)
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();
    NetworkGroup * group = groupManager->GetGroupByIndex(groupindex);
    return group->CanPerformAction(index);
}

int network_can_perform_command(unsigned int groupindex, unsigned int index) 
{
    INetworkContext * context = Network2::GetContext();
    INetworkGroupManager * groupManager = context->GetGroupManager();
    NetworkGroup * group = groupManager->GetGroupByIndex(groupindex);
    return group->CanPerformCommand(index);
}

int network_get_current_player_group_index()
{
    uint8 playerId = network_get_current_player_id();
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerById(playerId);
    return network_get_group_index(player->group);
}

void network_send_map()
{
    INetworkServer * server = Network2::GetServer();
    server->BroadcastMap();
}

void network_send_chat(const char * text)
{
    INetworkContext * context = Network2::GetContext();
    context->SendChatMessage(text);
}

void network_send_gamecmd(uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, uint32 esi, uint32 edi, uint32 ebp, uint8 callback)
{
    INetworkContext * context = Network2::GetContext();
    context->SendGameCommand(eax, ebx, ecx, edx, esi, edi, ebp, callback);
}

void network_send_password(const char * password)
{
    INetworkClient * client = Network2::GetClient();
    client->SendPassword(password);
}

void network_set_password(const char * password)
{
    INetworkServer * server = Network2::GetServer();
    server->SetPassword(password);
}

NetworkServerInfo network_get_server_info()
{
    INetworkContext * context = Network2::GetContext();
    return context->GetServerInfo();
}

static money32 game_command_set_player_group_2(uint8 playerId, uint8 groupId, uint8 flags)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerById(playerId);
    if (player != nullptr)
    {
        return MONEY32_UNDEFINED;
    }

    INetworkGroupManager * groupManager = context->GetGroupManager();
    NetworkGroup * fromGroup = groupManager->GetGroupById(game_command_playerid);
    if (!groupManager->GetGroupById(groupId))
    {
        return MONEY32_UNDEFINED;
    }

    if (player->flags & NETWORK_PLAYER_FLAG_ISSERVER)
    {
        gGameCommandErrorTitle = STR_CANT_CHANGE_GROUP_THAT_THE_HOST_BELONGS_TO;
        return MONEY32_UNDEFINED;
    }

    if (groupId == 0 && fromGroup && fromGroup->Id != 0)
    {
        gGameCommandErrorTitle = STR_CANT_SET_TO_THIS_GROUP;
        return MONEY32_UNDEFINED;
    }

    if (flags & GAME_COMMAND_FLAG_APPLY)
    {
        player->group = groupId;

        if (network_get_mode() == NETWORK_MODE_SERVER)
        {
            // Add or update saved user
            INetworkServer * server = static_cast<INetworkServer *>(context);
            INetworkUserManager * userManager = server->GetUserManager();
            NetworkUser * networkUser = userManager->GetOrAddUser(player->keyhash);
            networkUser->GroupId = groupId;
            networkUser->Name = player->name;
            userManager->Save();
        }

        window_invalidate_by_number(WC_PLAYER, playerId);
    }

    return 0;
}

void game_command_set_player_group(int * eax, int * ebx, int * ecx, int * edx, int * esi, int * edi, int * ebp)
{
    uint8 playerId = *ecx & 0xFF;
    uint8 groupId = *edx & 0xFF;
    uint8 flags = *ebx & 0xFF;
    *ebx = game_command_set_player_group_2(playerId, groupId, flags);
}

void game_command_modify_groups(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
    uint8 action = *eax & 0xFF;
    uint8 groupId = (*eax >> 8) & 0xFF;
    uint8 nameChunkIndex = (*eax >> 16) & 0xFF;

    char oldName[128];
    static char newName[128];

    switch (action) {
    case 0: // add group
    {
        if (*ebx & GAME_COMMAND_FLAG_APPLY)
        {
            INetworkGroupManager * groupManager = Network2::GetContext()
                                                          ->GetGroupManager();
            NetworkGroup * newGroup = groupManager->CreateGroup("???");
            if (newGroup != nullptr)
            {
                *ebx = MONEY32_UNDEFINED;
                return;
            }
        }
        break;
    }
    case 1: // remove group
    {
        if (groupId == 0)
        {
            gGameCommandErrorTitle = STR_THIS_GROUP_CANNOT_BE_MODIFIED;
            *ebx = MONEY32_UNDEFINED;
            return;
        }

        INetworkContext * context = Network2::GetContext();
        INetworkPlayerList * playerList = context->GetPlayerList();
        for (uint32 i = 0; i < playerList->GetCount(); i++)
        {
            NetworkPlayer * player = playerList->GetPlayerByIndex(i);
            if (player->group == groupId)
            {
                gGameCommandErrorTitle = STR_CANT_REMOVE_GROUP_THAT_PLAYERS_BELONG_TO;
                *ebx = MONEY32_UNDEFINED;
                return;
            }
        }
        if (*ebx & GAME_COMMAND_FLAG_APPLY)
        {
            INetworkGroupManager * groupManager = context->GetGroupManager();
            groupManager->RemoveGroup(groupId);

            if (network_get_mode() == NETWORK_MODE_SERVER)
            {
                auto server = static_cast<INetworkServer *>(context);
                INetworkUserManager * userManager = server->GetUserManager();
                userManager->UnsetUsersOfGroup(groupId);
                userManager->Save();
            }
        }
        break;
    }
    case 2: // set permissions
    {
        int index = *ecx;
        bool all = *edx & 1;
        bool allvalue = (*edx >> 1) & 1;
        if (groupId == 0)
        {
            // cant change admin group permissions
            gGameCommandErrorTitle = STR_THIS_GROUP_CANNOT_BE_MODIFIED;
            *ebx = MONEY32_UNDEFINED;
            return;
        }

        INetworkContext * context = Network2::GetContext();
        INetworkPlayerList * playerList = Network2::GetPlayerList();
        NetworkPlayer * player = playerList->GetPlayerById(game_command_playerid);
        INetworkGroupManager * groupManager = context->GetGroupManager();
        NetworkGroup * mygroup = nullptr;
        if (player != nullptr && !all)
        {
            mygroup = groupManager->GetGroupById(player->group);
            if (mygroup == nullptr || (mygroup != nullptr && !mygroup->CanPerformAction(index)))
            {
                gGameCommandErrorTitle = STR_CANT_MODIFY_PERMISSION_THAT_YOU_DO_NOT_HAVE_YOURSELF;
                *ebx = MONEY32_UNDEFINED;
                return;
            }
        }
        if (*ebx & GAME_COMMAND_FLAG_APPLY)
        {
            NetworkGroup * group = groupManager->GetGroupById(groupId);
            if (group != nullptr)
            {
                if (all)
                {
                    if (mygroup != nullptr)
                    {
                        if (allvalue)
                        {
                            group->ActionsAllowed = mygroup->ActionsAllowed;
                        }
                        else
                        {
                            group->ActionsAllowed.fill(0x00);
                        }
                    }
                }
                else
                {
                    group->ToggleActionPermission(index);
                }
            }
        }
        break;
    }
    case 3: // set group name
    {
        size_t nameChunkOffset = nameChunkIndex - 1;
        if (nameChunkIndex == 0)
        {
            nameChunkOffset = 2;
        }
        nameChunkOffset *= 12;
        nameChunkOffset = Math::Min(nameChunkOffset, Util::CountOf(newName) - 12);
        RCT2_GLOBAL(newName + nameChunkOffset + 0, uint32) = *edx;
        RCT2_GLOBAL(newName + nameChunkOffset + 4, uint32) = *ebp;
        RCT2_GLOBAL(newName + nameChunkOffset + 8, uint32) = *edi;

        if (nameChunkIndex != 0)
        {
            *ebx = 0;
            return;
        }

        if (String::Equals(oldName, newName) == 0)
        {
            *ebx = 0;
            return;
        }

        if (newName[0] == '\0')
        {
            gGameCommandErrorTitle = STR_CANT_RENAME_GROUP;
            gGameCommandErrorText = STR_INVALID_GROUP_NAME;
            *ebx = MONEY32_UNDEFINED;
            return;
        }

        if (*ebx & GAME_COMMAND_FLAG_APPLY)
        {
            INetworkGroupManager * groupManager = Network2::GetContext()
                                                          ->GetGroupManager();
            NetworkGroup * group = groupManager->GetGroupById(groupId);
            if (group != nullptr)
            {
                group->SetName(newName);
            }
        }
        break;
    }
    case 4: // set default group
    {
        if (groupId == 0)
        {
            gGameCommandErrorTitle = STR_CANT_SET_TO_THIS_GROUP;
            *ebx = MONEY32_UNDEFINED;
            return;
        }
        if (*ebx & GAME_COMMAND_FLAG_APPLY)
        {
            INetworkGroupManager * groupManager = Network2::GetContext()
                                                          ->GetGroupManager();
            groupManager->SetDefaultGroupId(groupId);
        }
        break;
    }
    }

    {
        INetworkGroupManager * groupManager = Network2::GetContext()
            ->GetGroupManager();
        groupManager->Save();
    }

    *ebx = 0;
}

static money32 game_command_kick_player_2(uint8 playerId, uint8 flags)
{
    INetworkContext * context = Network2::GetContext();
    INetworkPlayerList * playerList = context->GetPlayerList();
    NetworkPlayer * player = playerList->GetPlayerById(playerId);
    if (player != nullptr && player->flags & NETWORK_PLAYER_FLAG_ISSERVER)
    {
        gGameCommandErrorTitle = STR_CANT_KICK_THE_HOST;
        return MONEY32_UNDEFINED;
    }

    if (flags & GAME_COMMAND_FLAG_APPLY)
    {
        if (network_get_mode() == NETWORK_MODE_SERVER)
        {
            INetworkServer * server = static_cast<INetworkServer *>(context);
            server->KickPlayer(playerId);

            INetworkUserManager * networkUserManager = server->GetUserManager();
            networkUserManager->Load();
            networkUserManager->RemoveUser(player->keyhash);
            networkUserManager->Save();
        }
    }

    return 0;
}

void game_command_kick_player(int * eax, int * ebx, int * ecx, int * edx, int * esi, int * edi, int * ebp)
{
    uint8 playerId = *eax & 0xFF;
    uint8 flags = *ebx & 0xFF;
    *ebx = game_command_kick_player_2(playerId, flags);
}

}

#else

int network_get_mode() { return NETWORK_MODE_NONE; }
int network_get_status() { return NETWORK_STATUS_NONE; }
int network_get_authstatus() { return NETWORK_AUTH_NONE; }
uint32 network_get_server_tick() { return gCurrentTicks; }
void network_send_gamecmd(uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, uint32 esi, uint32 edi, uint32 ebp, uint8 callback) {}
void network_send_map() {}
void network_update() {}
int network_begin_client(const char *host, int port) { return 1; }
int network_begin_server(int port) { return 1; }
int network_get_num_players() { return 1; }
const char* network_get_player_name(unsigned int index) { return "local (OpenRCT2 compiled without MP)"; }
uint32 network_get_player_flags(unsigned int index) { return 0; }
int network_get_player_ping(unsigned int index) { return 0; }
int network_get_player_id(unsigned int index) { return 0; }
money32 network_get_player_money_spent(unsigned int index) { return MONEY(0, 0); }
void network_add_player_money_spent(unsigned int index, money32 cost) { }
int network_get_player_last_action(unsigned int index, int time) { return -999; }
void network_set_player_last_action(unsigned int index, int command) { }
rct_xyz16 network_get_player_last_action_coord(unsigned int index) { return {0, 0, 0}; }
void network_set_player_last_action_coord(unsigned int index, rct_xyz16 coord) { }
unsigned int network_get_player_commands_ran(unsigned int index) { return 0; }
int network_get_player_index(uint8 id) { return -1; }
uint8 network_get_player_group(unsigned int index) { return 0; }
int network_get_group_index(uint8 id) { return -1; }
uint8 network_get_group_id(unsigned int index) { return 0; }
int network_get_num_groups() { return 0; }
const char* network_get_group_name(unsigned int index) { return ""; };
void game_command_set_player_group(int* eax, int* ebx, int* ecx, int* edx, int* esi, int* edi, int* ebp) { }
void game_command_modify_groups(int* eax, int* ebx, int* ecx, int* edx, int* esi, int* edi, int* ebp) { }
void game_command_kick_player(int* eax, int* ebx, int* ecx, int* edx, int* esi, int* edi, int* ebp) { }
uint8 network_get_default_group() { return 0; }
int network_get_num_actions() { return 0; }
rct_string_id network_get_action_name_string_id(unsigned int index) { return -1; }
int network_can_perform_action(unsigned int groupindex, unsigned int index) { return 0; }
int network_can_perform_command(unsigned int groupindex, unsigned int index) { return 0; }
void network_send_chat(const char* text) {}
void network_send_password(const char* password) {}
void network_close() {}
void network_shutdown_client() {}
void network_set_password(const char* password) {}
uint8 network_get_current_player_id() { return 0; }
int network_get_current_player_group_index() { return 0; }
NetworkServerInfo network_get_server_info() { return { 0 }; }

}

#endif /* DISABLE_NETWORK */
