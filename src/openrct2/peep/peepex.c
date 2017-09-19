#include "peepex.h"
#include "../scenario/scenario.h"
#include "../config/Config.h"

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