#include "peepex.h"
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
	int preferenceDirection = peep->peeps_ex_direction_preference;
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
	peep->peeps_ex_direction_preference = spriteDirection;

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