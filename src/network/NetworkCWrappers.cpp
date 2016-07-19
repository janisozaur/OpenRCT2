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

#include "network.h"
#include "Network2.h"
#include "NetworkServer.h"

int network_init()
{
    Network2::Initialise();
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
	return gNetwork.GetStatus();
}

int network_get_authstatus()
{
	return gNetwork.GetAuthStatus();
}

uint32 network_get_server_tick()
{
	return gNetwork.GetServerTick();
}

uint8 network_get_current_player_id()
{
	return gNetwork.GetPlayerID();
}

int network_get_num_players()
{
	return gNetwork.player_list.size();
}

const char* network_get_player_name(unsigned int index)
{
	return (const char*)gNetwork.player_list[index]->name.c_str();
}

uint32 network_get_player_flags(unsigned int index)
{
	return gNetwork.player_list[index]->flags;
}

int network_get_player_ping(unsigned int index)
{
	return gNetwork.player_list[index]->ping;
}

int network_get_player_id(unsigned int index)
{
	return gNetwork.player_list[index]->id;
}

money32 network_get_player_money_spent(unsigned int index)
{
	return gNetwork.player_list[index]->money_spent;
}

void network_add_player_money_spent(unsigned int index, money32 cost)
{
	gNetwork.player_list[index]->AddMoneySpent(cost);
}

int network_get_player_last_action(unsigned int index, int time)
{
	if (time && SDL_TICKS_PASSED(SDL_GetTicks(), gNetwork.player_list[index]->last_action_time + time)) {
		return -999;
	}
	return gNetwork.player_list[index]->last_action;
}

void network_set_player_last_action(unsigned int index, int command)
{
	gNetwork.player_list[index]->last_action = NetworkActions::FindCommand(command);
	gNetwork.player_list[index]->last_action_time = SDL_GetTicks();
}

rct_xyz16 network_get_player_last_action_coord(unsigned int index)
{
	return gNetwork.player_list[index]->last_action_coord;
}

void network_set_player_last_action_coord(unsigned int index, rct_xyz16 coord)
{
	if (index < gNetwork.player_list.size()) {
		gNetwork.player_list[index]->last_action_coord = coord;
	}
}

unsigned int network_get_player_commands_ran(unsigned int index)
{
	return gNetwork.player_list[index]->commands_ran;
}

int network_get_player_index(uint8 id)
{
	auto it = gNetwork.GetPlayerIteratorByID(id);
	if(it == gNetwork.player_list.end()){
		return -1;
	}
	return gNetwork.GetPlayerIteratorByID(id) - gNetwork.player_list.begin();
}

uint8 network_get_player_group(unsigned int index)
{
	return gNetwork.player_list[index]->group;
}

void network_set_player_group(unsigned int index, unsigned int groupindex)
{
	gNetwork.player_list[index]->group = gNetwork.group_list[groupindex]->Id;
}

int network_get_group_index(uint8 id)
{
	auto it = gNetwork.GetGroupIteratorByID(id);
	if(it == gNetwork.group_list.end()){
		return -1;
	}
	return gNetwork.GetGroupIteratorByID(id) - gNetwork.group_list.begin();
}

uint8 network_get_group_id(unsigned int index)
{
	return gNetwork.group_list[index]->Id;
}

int network_get_num_groups()
{
	return gNetwork.group_list.size();
}

const char* network_get_group_name(unsigned int index)
{
	return gNetwork.group_list[index]->GetName().c_str();
}

void network_chat_show_connected_message()
{
	// TODO: How does this work? 2525 is '???'
	char *templateString = (char*)language_get_string(STR_SHORTCUT_KEY_UNKNOWN);
	keyboard_shortcut_format_string(templateString, gShortcutKeys[SHORTCUT_OPEN_CHAT_WINDOW]);
	utf8 buffer[256];
	NetworkPlayer server;
	server.name = "Server";
	format_string(buffer, STR_MULTIPLAYER_CONNECTED_CHAT_HINT, &templateString);
	const char *formatted = Network::FormatChat(&server, buffer);
	chat_history_add(formatted);
}

void game_command_set_player_group(int* eax, int* ebx, int* ecx, int* edx, int* esi, int* edi, int* ebp)
{
	uint8 playerid = (uint8)*ecx;
	uint8 groupid = (uint8)*edx;
	NetworkPlayer* player = gNetwork.GetPlayerByID(playerid);
	NetworkGroup* fromgroup = gNetwork.GetGroupByID(game_command_playerid);
	if (!player) {
		*ebx = MONEY32_UNDEFINED;
		return;
	}
	if (!gNetwork.GetGroupByID(groupid)) {
		*ebx = MONEY32_UNDEFINED;
		return;
	}
	if (player->flags & NETWORK_PLAYER_FLAG_ISSERVER) {
		gGameCommandErrorTitle = STR_CANT_CHANGE_GROUP_THAT_THE_HOST_BELONGS_TO;
		*ebx = MONEY32_UNDEFINED;
		return;
	}
	if (groupid == 0 && fromgroup && fromgroup->Id != 0) {
		gGameCommandErrorTitle = STR_CANT_SET_TO_THIS_GROUP;
		*ebx = MONEY32_UNDEFINED;
		return;
	}
	if (*ebx & GAME_COMMAND_FLAG_APPLY) {
		player->group = groupid;

		if (network_get_mode() == NETWORK_MODE_SERVER) {
			// Add or update saved user
			NetworkUserManager *userManager = &gNetwork._userManager;
			NetworkUser *networkUser = userManager->GetOrAddUser(player->keyhash);
			networkUser->GroupId = groupid;
			networkUser->Name = player->name;
			userManager->Save();
		}

		window_invalidate_by_number(WC_PLAYER, playerid);
	}
	*ebx = 0;
}

void game_command_modify_groups(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
	uint8 action = (uint8)*eax;
	uint8 groupid = (uint8)(*eax >> 8);
	uint8 nameChunkIndex = (uint8)(*eax >> 16);

	char oldName[128];
	static char newName[128];

	switch (action)
	{
	case 0:{ // add group
		if (*ebx & GAME_COMMAND_FLAG_APPLY) {
			NetworkGroup* newgroup = gNetwork.AddGroup();
			if (!newgroup) {
				*ebx = MONEY32_UNDEFINED;
				return;
			}
		}
	}break;
	case 1:{ // remove group
		if (groupid == 0) {
			gGameCommandErrorTitle = STR_THIS_GROUP_CANNOT_BE_MODIFIED;
			*ebx = MONEY32_UNDEFINED;
			return;
		}
		for (auto it = gNetwork.player_list.begin(); it != gNetwork.player_list.end(); it++) {
			if((*it)->group == groupid) {
				gGameCommandErrorTitle = STR_CANT_REMOVE_GROUP_THAT_PLAYERS_BELONG_TO;
				*ebx = MONEY32_UNDEFINED;
				return;
			}
		}
		if (*ebx & GAME_COMMAND_FLAG_APPLY) {
			gNetwork.RemoveGroup(groupid);
		}
	}break;
	case 2:{ // set permissions
		int index = *ecx;
		bool all = *edx & 1;
		bool allvalue = (*edx >> 1) & 1;
		if (groupid == 0) { // cant change admin group permissions
			gGameCommandErrorTitle = STR_THIS_GROUP_CANNOT_BE_MODIFIED;
			*ebx = MONEY32_UNDEFINED;
			return;
		}
		NetworkGroup* mygroup = nullptr;
		NetworkPlayer* player = gNetwork.GetPlayerByID(game_command_playerid);
		if (player && !all) {
			mygroup = gNetwork.GetGroupByID(player->group);
			if (!mygroup || (mygroup && !mygroup->CanPerformAction(index))) {
				gGameCommandErrorTitle = STR_CANT_MODIFY_PERMISSION_THAT_YOU_DO_NOT_HAVE_YOURSELF;
				*ebx = MONEY32_UNDEFINED;
				return;
			}
		}
		if (*ebx & GAME_COMMAND_FLAG_APPLY) {
			NetworkGroup* group = gNetwork.GetGroupByID(groupid);
			if (group) {
				if (all) {
					if (mygroup) {
						if (allvalue) {
							group->ActionsAllowed = mygroup->ActionsAllowed;
						} else {
							group->ActionsAllowed.fill(0x00);
						}
					}
				} else {
					group->ToggleActionPermission(index);
				}
			}
		}
	}break;
	case 3:{ // set group name
		size_t nameChunkOffset = nameChunkIndex - 1;
		if (nameChunkIndex == 0)
			nameChunkOffset = 2;
		nameChunkOffset *= 12;
		nameChunkOffset = (std::min)(nameChunkOffset, Util::CountOf(newName) - 12);
		RCT2_GLOBAL(newName + nameChunkOffset + 0, uint32) = *edx;
		RCT2_GLOBAL(newName + nameChunkOffset + 4, uint32) = *ebp;
		RCT2_GLOBAL(newName + nameChunkOffset + 8, uint32) = *edi;

		if (nameChunkIndex != 0) {
			*ebx = 0;
			return;
		}

		if (strcmp(oldName, newName) == 0) {
			*ebx = 0;
			return;
		}

		if (newName[0] == 0) {
			gGameCommandErrorTitle = STR_CANT_RENAME_GROUP;
			gGameCommandErrorText = STR_INVALID_GROUP_NAME;
			*ebx = MONEY32_UNDEFINED;
			return;
		}

		if (*ebx & GAME_COMMAND_FLAG_APPLY) {
			NetworkGroup* group = gNetwork.GetGroupByID(groupid);
			if (group) {
				group->SetName(newName);
			}
		}
	}break;
	case 4:{ // set default group
		if (groupid == 0) {
			gGameCommandErrorTitle = STR_CANT_SET_TO_THIS_GROUP;
			*ebx = MONEY32_UNDEFINED;
			return;
		}
		if (*ebx & GAME_COMMAND_FLAG_APPLY) {
			gNetwork.SetDefaultGroup(groupid);
		}
	}break;
	}

	gNetwork.SaveGroups();

	*ebx = 0;
}

void game_command_kick_player(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp)
{
	uint8 playerid = (uint8)*eax;
	NetworkPlayer* player = gNetwork.GetPlayerByID(playerid);
	if (player && player->flags & NETWORK_PLAYER_FLAG_ISSERVER) {
		gGameCommandErrorTitle = STR_CANT_KICK_THE_HOST;
		*ebx = MONEY32_UNDEFINED;
		return;
	}
	if (*ebx & GAME_COMMAND_FLAG_APPLY) {
		if (gNetwork.GetMode() == NETWORK_MODE_SERVER) {
			gNetwork.KickPlayer(playerid);

			NetworkUserManager * networkUserManager = &gNetwork._userManager;
			networkUserManager->Load();
			networkUserManager->RemoveUser(player->keyhash);
			networkUserManager->Save();
		}
	}
	*ebx = 0;
}

uint8 network_get_default_group()
{
	return gNetwork.GetDefaultGroup();
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
	return gNetwork.group_list[groupindex]->CanPerformAction(index);
}

int network_can_perform_command(unsigned int groupindex, unsigned int index) 
{
	return gNetwork.group_list[groupindex]->CanPerformCommand(index);
}

int network_get_current_player_group_index()
{
	return network_get_group_index(gNetwork.GetPlayerByID(gNetwork.GetPlayerID())->group);
}

void network_send_map()
{
	gNetwork.Server_Send_MAP();
}

void network_send_chat(const char* text)
{
	if (gNetwork.GetMode() == NETWORK_MODE_CLIENT) {
		gNetwork.Client_Send_CHAT(text);
	} else
	if (gNetwork.GetMode() == NETWORK_MODE_SERVER) {
		NetworkPlayer* player = gNetwork.GetPlayerByID(gNetwork.GetPlayerID());
		const char* formatted = gNetwork.FormatChat(player, text);
		chat_history_add(formatted);
		gNetwork.Server_Send_CHAT(formatted);
	}
}

void network_send_gamecmd(uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, uint32 esi, uint32 edi, uint32 ebp, uint8 callback)
{
	switch (gNetwork.GetMode()) {
	case NETWORK_MODE_SERVER:
		gNetwork.Server_Send_GAMECMD(eax, ebx, ecx, edx, esi, edi, ebp, gNetwork.GetPlayerID(), callback);
		break;
	case NETWORK_MODE_CLIENT:
		gNetwork.Client_Send_GAMECMD(eax, ebx, ecx, edx, esi, edi, ebp, callback);
		break;
	}
}

void network_send_password(const char* password)
{
	utf8 keyPath[MAX_PATH];
	network_get_private_key_path(keyPath, sizeof(keyPath), gConfigNetwork.player_name);
	if (!platform_file_exists(keyPath)) {
		log_error("Private key %s missing! Restart the game to generate it.", keyPath);
		return;
	}
	SDL_RWops *privkey = SDL_RWFromFile(keyPath, "rb");
	gNetwork.key.LoadPrivate(privkey);
	const std::string pubkey = gNetwork.key.PublicKeyString();
	size_t sigsize;
	char *signature;
	gNetwork.key.Sign(gNetwork.challenge.data(), gNetwork.challenge.size(), &signature, &sigsize);
	// Don't keep private key in memory. There's no need and it may get leaked
	// when process dump gets collected at some point in future.
	gNetwork.key.Unload();
	gNetwork.Client_Send_AUTH(gConfigNetwork.player_name, password, pubkey.c_str(), signature, sigsize);
	delete [] signature;
}

void network_set_password(const char* password)
{
	gNetwork.SetPassword(password);
}

void network_append_chat_log(const utf8 *text)
{
	gNetwork.AppendChatLog(text);
}

static void network_get_keys_directory(utf8 *buffer, size_t bufferSize)
{
	platform_get_user_directory(buffer, "keys");
}

static void network_get_private_key_path(utf8 *buffer, size_t bufferSize, const utf8 * playerName)
{
	network_get_keys_directory(buffer, bufferSize);
	Path::Append(buffer, bufferSize, playerName);
	String::Append(buffer, bufferSize, ".privkey");
}

static void network_get_public_key_path(utf8 *buffer, size_t bufferSize, const utf8 * playerName, const utf8 * hash)
{
	network_get_keys_directory(buffer, bufferSize);
	Path::Append(buffer, bufferSize, playerName);
	String::Append(buffer, bufferSize, "-");
	String::Append(buffer, bufferSize, hash);
	String::Append(buffer, bufferSize, ".pubkey");
}

NetworkServerInfo network_get_server_info()
{
    INetworkContext * context = Network2::GetContext();
    return context->GetServerInfo();
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
void network_set_player_group(unsigned int index, unsigned int groupindex) { }
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
void network_append_chat_log(const utf8 *text) { }
NetworkServerInfo network_get_server_info() { return { 0 }; }

#endif /* DISABLE_NETWORK */
