#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
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

#ifndef _PEEPEX_H_
#define _PEEPEX_H_

#include "peep.h"

void peepex_base_update(rct_peep *peep, sint32 index);
void peep_update_action_sidestepping(sint16* x, sint16* y, sint16 x_delta, sint16 y_delta, sint16* xy_distance, rct_peep* peep);
sint32 peep_update_queue_position_messy(rct_peep* peep, uint8 previous_action);
sint32 peep_move_one_tile_messy(sint32 x, sint32 y, uint8 direction, rct_peep* peep);

#endif