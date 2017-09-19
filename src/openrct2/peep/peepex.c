#include "peepex.h"
#include "../ride/ride.h"
#include "../scenario/scenario.h"
#include "../config/Config.h"

/** rct2: 0x00981D7C, 0x00981D7E */
const rct_xy16 peepex_directional[4] = {
    { -2,  0 },
    {  0,  2 },
    {  2,  0 },
    {  0, -2 }
};

void peepex_base_update(rct_peep *peep, sint32 index)
{
		// If guests are idly walking and they have spent a lot of time here, make them always leave.

	if (gConfigPeepEx.guest_max_time_in_park > 0 && peep->state == PEEP_STATE_WALKING) {

		uint32 time_duration = gScenarioTicks - peep->time_in_park;

		if ((time_duration >> 11) > (uint32)gConfigPeepEx.guest_max_time_in_park) {
			peep_leave_park(peep);
		}
	}
}

void peep_update_action_sidestepping(sint16* x, sint16* y, sint16 x_delta, sint16 y_delta, sint16* xy_distance, rct_peep* peep)
{
	int spriteDirection = 0;
	int preferenceDirection = peep->peepex_direction_preference;
	int sidestepX = 0;
	int sidestepY = 0;
			
	int x_delta_weight = x_delta + ((preferenceDirection == 16 || preferenceDirection == 0)? max(0, x_delta - peep->destination_tolerence) * 3 : 0);
	int y_delta_weight = y_delta + ((preferenceDirection == 8 || preferenceDirection == 24)? max(0, y_delta - peep->destination_tolerence) * 3 : 0);

	if (y_delta_weight > x_delta_weight) {
		spriteDirection = 8;
		if (*y >= 0) {
			spriteDirection = 24;
		}

		if (scenario_rand_max(*xy_distance) < (uint32)x_delta * 1) {
			if (*x > 1)
				sidestepX = -1;
			else if (*x < -1)
				sidestepX = 1;
		}
	}
	else {
		spriteDirection = 16;
		if (*x >= 0) {
			spriteDirection = 0;
		}
		if (scenario_rand_max(*xy_distance) < (uint32)y_delta * 1) {
			if (*y > 1)
				sidestepY = -1;
			else if (*y < -1)
				sidestepY = 1;
		}
	}

	peep->sprite_direction = preferenceDirection;
	peep->peepex_direction_preference = spriteDirection;

	sint16 actualStepX = peepex_directional[spriteDirection / 8].x;
	sint16 actualStepY = peepex_directional[spriteDirection / 8].y;

	if (sidestepX != 0) {
		actualStepY += (actualStepY & 0x8000)? 1 : -1;
	}
	else if (sidestepY != 0) {
		actualStepX += (actualStepX & 0x8000)? 1 : -1;
	}

	*x = peep->x + actualStepX + sidestepX;
	*y = peep->y + actualStepY + sidestepY;
}

sint32 peep_update_queue_position_messy(rct_peep* peep, uint8 previous_action)
{
	rct_peep*	peep_next = GET_PEEP(peep->next_in_queue);
	Ride*		peep_ride = get_ride(peep->current_ride);

	sint16 x_diff = abs(peep_next->x - peep->x);
	sint16 y_diff = abs(peep_next->y - peep->y);
	sint16 z_diff = abs(peep_next->z - peep->z);

	if (z_diff > 30)
 		return 0;

	if (x_diff < y_diff){
		sint16 temp_x = x_diff;
		x_diff = y_diff;
		y_diff = temp_x;
	}

	x_diff += y_diff / 2;

		// At the last tile, it's best not to do any strange things.
		//	this variable checks if there is a tile ahead (will not be
		//	checked more often than once)

	int		prohibitShenanigans	= 1;
	int		peepsAhead			= 0;
	bool	allowMove			= true;

	uint16 macroCurX = peep->x >> 5;
	uint16 macroCurY = peep->y >> 5;
	uint16 macroDesX = peep->destination_x >> 5;
	uint16 macroDesY = peep->destination_y >> 5;
	uint16 macroCurNxX = peep_next->x >> 5;
	uint16 macroCurNxY = peep_next->y >> 5;

	if (peep_next->next_in_queue != 0xFFFF) {
		rct_peep *nxtCue = peep_next;

		while (true) {
			if (nxtCue->next_in_queue == 0xFFFF) {
				break;
			}
			nxtCue = GET_PEEP(nxtCue->next_in_queue);

			peepsAhead += 1;

			uint16 macroCurFarX = nxtCue->x >> 5;
			uint16 macroCurFarY = nxtCue->y >> 5;

			if (macroCurFarX == macroCurNxX &&
				macroCurFarY == macroCurNxY) {
				continue;
			}

				// Someone is ahead of the person blocking us, in another tile;
				//	we could never, ever share that tile!

			if (macroCurX == macroCurFarX &&
				macroCurY == macroCurFarY) {
				allowMove = false;
			}

			if (prohibitShenanigans > 0)
				prohibitShenanigans -= 1;

			while (peepsAhead < 40) {
				if (nxtCue->next_in_queue == 0xFFFF) {
					break;
				}
				nxtCue = GET_PEEP(nxtCue->next_in_queue);
				peepsAhead += 1;
			}

			break;
		}
	}

	// Are we moving? Take up more space! Are we not? Reduce space!
	// Are we and the next both stopped? Nudge hir to queue up better.

	// The most significant bit in peeps_ex_queue_wait_distance contains
	//	whether a peep was moving. This is used to 'propagate' peeps
	//	waiting close-by one-another.

	uint32 tickRef = peep->time_in_queue;

	if (tickRef % 11 == 0) {
		if (peep->peepex_queue_wait_distance & 0x80) {
			if ((peep->peepex_queue_wait_distance & 0x7F) < 14) {
				peep->peepex_queue_wait_distance += scenario_rand_max(6);
				if ((macroCurNxX != macroCurX || macroCurNxY != macroCurY) &&
					(macroCurNxX != macroDesX || macroCurNxY != macroDesY)) {
					peep->peepex_queue_wait_distance += 2;
				}
			}
		}
	}
	if (tickRef % 23 == 0) {
		if (!(peep_next->peepex_queue_wait_distance & 0x80)) {
			if (peep->peepex_queue_wait_distance < peep_next->peepex_queue_wait_distance) {
				peep_next->peepex_queue_wait_distance = max(7, min(peep_next->peepex_queue_wait_distance, peep->peepex_queue_wait_distance + scenario_rand_max(2)));
			}
			if (!(peep->peepex_queue_wait_distance & 0x80)) {
				int randomCount = 2048;
				uint32 randNum = scenario_rand_max(16 * randomCount);
				int customerCheck = 5 + peep_ride->cur_num_customers / 2;
				uint32 decreaseCheck = min(randomCount, (customerCheck - max(customerCheck, peepsAhead) * (randomCount / 16)));
				if ((randNum % randomCount) <= decreaseCheck) {
					peep->peepex_queue_wait_distance -= randNum / randomCount;
					peep->peepex_queue_wait_distance = max(7, peep->peepex_queue_wait_distance);
				}
			}
		}
		if ((peep->peepex_queue_wait_distance & 0x7F) < 6) {
			peep->peepex_queue_wait_distance += 1;
		}
	}

	if (allowMove) {	
		peep_next	= peep;

		for (int ahead = 0; ahead <3; ahead++) {
			if (peep_next->next_in_queue == 0xFFFF) {
				break;
			}
			peep_next = GET_PEEP(peep_next->next_in_queue);

			x_diff = abs(peep_next->x - peep->x);
			y_diff = abs(peep_next->y - peep->y);

			if (x_diff < y_diff) {
				sint16 temp_x = x_diff;
				x_diff = y_diff;
				y_diff = temp_x;
			}

			x_diff += y_diff / 2;

			if (x_diff < (0x7F & peep->peepex_queue_wait_distance)) {
				allowMove = false;
				break;
			}
		}
	}
	if (allowMove) {
			// State that we are moving
		peep->peepex_queue_wait_distance |= 0x80;

		if (prohibitShenanigans == 0) {
			if (macroDesX == peep_next->x >> 5 &&
				macroDesY == peep_next->y >> 5) {

				uint16 tarX = peep_next->x;
				uint16 tarY = peep_next->y;

				if (tickRef % 3 == 0) {
					int maxOffset = 5;
					
					tarX += -maxOffset + scenario_rand_max(2 * maxOffset + 1);
					tarY += -maxOffset + scenario_rand_max(2 * maxOffset + 1);
				}

				if (macroDesX != macroCurX) {
					if (macroDesX > macroCurX) {
						peep->destination_x = max((macroDesX << 5) + 10, tarX);
					}
					else {
						peep->destination_x = min((macroDesX << 5) + 22, tarX);
					}

					peep->destination_y = min(max((macroDesY << 5) + 10, tarY), (macroDesY << 5) + 22);
				}
				else if (macroDesY != macroCurY) {
					if (macroDesY > macroCurY) {
						peep->destination_y = max((macroDesY << 5) + 10, tarY);
					}
					else {
						peep->destination_y = min((macroDesY << 5) + 22, tarY);
					}

					peep->destination_x = min(max((macroDesX << 5) + 10, tarX), (macroDesX << 5) + 22);
				}

				peep->destination_tolerence = 2;
			}
			else {
				sint16 x = peep->destination_x & 0xFFE0;
				sint16 y = peep->destination_y & 0xFFE0;

				sint16 tileOffsetX = peep->destination_x & ~0xFFE0;
				sint16 tileOffsetY = peep->destination_y & ~0xFFE0;

				tileOffsetX -= 16;
				tileOffsetY -= 16;

				tileOffsetX += (peep->x - peep->destination_x) / 3;
				tileOffsetY += (peep->y - peep->destination_y) / 3;

				if (tickRef % 3 == 0) {
					tileOffsetX += -7 + scenario_rand_max(15);
					tileOffsetY += -7 + scenario_rand_max(15);
				}

				sint16 maxTileOffset = 6;

				if (tileOffsetX < -maxTileOffset)
					tileOffsetX = -maxTileOffset;
				else if (tileOffsetX > maxTileOffset)
					tileOffsetX = maxTileOffset;
				if (tileOffsetY < -maxTileOffset)
					tileOffsetY = -maxTileOffset;
				else if (tileOffsetY > maxTileOffset)
					tileOffsetY = maxTileOffset;

				peep->destination_x = x + tileOffsetX + 16;
				peep->destination_y = y + tileOffsetY + 16;

				peep->destination_tolerence = 3;
			}
		}

		return 0;
	}
	else {
			// We are not moving
		peep->peepex_queue_wait_distance &= 0x7F;
 	}

	return 1;
}

sint32 peep_move_one_tile_messy(sint32 x, sint32 y, uint8 direction, rct_peep* peep)
{	
	sint16 tileOffsetX = peep->x & ~0xFFE0;
	sint16 tileOffsetY = peep->y & ~0xFFE0;

	tileOffsetX -= 16;
	tileOffsetY -= 16;

	tileOffsetX -= scenario_rand_max(TileDirectionDelta[direction].x);
	tileOffsetY -= scenario_rand_max(TileDirectionDelta[direction].y);

	if (peep->state == PEEP_STATE_WALKING) {
		sint16 enterOffsetX = 0;
		sint16 enterOffsetY = 0;

		{
			switch (peep->peepex_direction_preference) {
			case 0:
				enterOffsetX = -7 + scenario_rand_max(4);
				break;
			case 8:
				enterOffsetY = 7 + scenario_rand_max(4);
				break;
			case 16:
				enterOffsetX = 7 + scenario_rand_max(4);
				break;
			case 24:
				enterOffsetY = -7 + scenario_rand_max(4);
				break;
			default:
				break;
			};
		}

		tileOffsetX += enterOffsetX;
		tileOffsetY += enterOffsetY;

		if (peep->peepex_crowded_store > 0) {
			uint8 crowdedAdd = max((sint16)(50) - (sint16)(peep->peepex_crowded_store), (sint16)(1));
			tileOffsetX += TileDirectionDelta[(direction+1) & 0x3].x / crowdedAdd;
			tileOffsetY += TileDirectionDelta[(direction+1) & 0x3].y / crowdedAdd;
		}

		int offset = 1 + min(10, max(0, (peep->nausea / 50) - 3) + (peep->peepex_crowded_store / 20));

		tileOffsetX += -offset + scenario_rand_max(1 + (offset << 1));
		tileOffsetY += -offset + scenario_rand_max(1 + (offset << 1));

		sint16 maxTileOffset = 6;

		maxTileOffset += min(peep->peepex_crowded_store / 3, 5);

		if (tileOffsetX < -maxTileOffset)
			tileOffsetX = -maxTileOffset;
		else if (tileOffsetX > maxTileOffset)
			tileOffsetX = maxTileOffset;
		if (tileOffsetY < -maxTileOffset)
			tileOffsetY = -maxTileOffset;
		else if (tileOffsetY > maxTileOffset)
			tileOffsetY = maxTileOffset;
	}
	else {
		tileOffsetX = 0;
		tileOffsetY = 0;
	}

	peep->direction = direction;
	peep->destination_x = x + tileOffsetX + 16;
	peep->destination_y = y + tileOffsetY + 16;
	peep->destination_tolerence = 2;
	if (peep->state != PEEP_STATE_QUEUING) {
		peep->destination_tolerence = 4;
	}
	else {
		peep->destination_tolerence = 2;
	}

	return 0;
}