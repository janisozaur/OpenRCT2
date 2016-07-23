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

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "Network2.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include "../world/map.h"

int network_init();
void network_close();
void network_shutdown_client();
int network_begin_client(const char *host, int port);
int network_begin_server(int port);

int network_get_mode();
int network_get_status();
void network_update();
int network_get_authstatus();
uint32 network_get_server_tick();
uint8 network_get_current_player_id();
int network_get_num_players();
const char* network_get_player_name(unsigned int index);
uint32 network_get_player_flags(unsigned int index);
int network_get_player_ping(unsigned int index);
int network_get_player_id(unsigned int index);
money32 network_get_player_money_spent(unsigned int index);
void network_add_player_money_spent(unsigned int index, money32 cost);
int network_get_player_last_action(unsigned int index, int time);
void network_set_player_last_action(unsigned int index, int command);
rct_xyz16 network_get_player_last_action_coord(unsigned int index);
void network_set_player_last_action_coord(unsigned int index, rct_xyz16 coord);
unsigned int network_get_player_commands_ran(unsigned int index);
int network_get_player_index(uint8 id);
uint8 network_get_player_group(unsigned int index);
int network_get_group_index(uint8 id);
int network_get_current_player_group_index();
uint8 network_get_group_id(unsigned int index);
int network_get_num_groups();
const char* network_get_group_name(unsigned int index);
void game_command_set_player_group(int* eax, int* ebx, int* ecx, int* edx, int* esi, int* edi, int* ebp);
void game_command_modify_groups(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp);
void game_command_kick_player(int *eax, int *ebx, int *ecx, int *edx, int *esi, int *edi, int *ebp);
uint8 network_get_default_group();
int network_get_num_actions();
rct_string_id network_get_action_name_string_id(unsigned int index);
int network_can_perform_action(unsigned int groupindex, unsigned int index);
int network_can_perform_command(unsigned int groupindex, unsigned int index);

void network_send_map();
void network_send_chat(const char* text);
void network_send_gamecmd(uint32 eax, uint32 ebx, uint32 ecx, uint32 edx, uint32 esi, uint32 edi, uint32 ebp, uint8 callback);
void network_send_password(const char* password);

void network_set_password(const char* password);

void network_print_error();
void network_append_chat_log(const utf8 *text);
NetworkServerInfo network_get_server_info();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
