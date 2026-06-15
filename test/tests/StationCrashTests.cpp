/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideManager.hpp>
#include <openrct2/ride/Station.h>
#include <openrct2/ride/ted/TrackElemType.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/tile_element/TrackElement.h>

using namespace OpenRCT2;

TEST(StationCrashTests, ValidateStationsInfiniteLoop)
{
    gOpenRCT2Headless = true;
    gOpenRCT2NoGraphics = true;
    auto context = CreateContext();
    ASSERT_TRUE(context->Initialise());

    MapInit({ 64, 64 });

    RideId rideId = GetNextFreeRideId();
    Ride* ride = RideAllocateAtIndex(rideId);
    ride->type = RIDE_TYPE_JUNIOR_ROLLER_COASTER;

    // Station Piece A at (32, 32), Direction 2 (East)
    // Back of A is at (32-1, 32) = (31, 32)
    auto* trackA = TileElementInsert<TrackElement>(CoordsXYZ(32 * kCoordsXYStep, 32 * kCoordsXYStep, 32), 0xFF);
    trackA->SetRideIndex(rideId);
    trackA->SetTrackType(TrackElemType::endStation);
    trackA->setDirection(2);
    trackA->baseHeight = 32;

    // Station Piece B at (31, 32), Direction 0 (West)
    // Back of B is at (31+1, 32) = (32, 32)
    auto* trackB = TileElementInsert<TrackElement>(CoordsXYZ(31 * kCoordsXYStep, 32 * kCoordsXYStep, 32), 0xFF);
    trackB->SetRideIndex(rideId);
    trackB->SetTrackType(TrackElemType::endStation);
    trackB->setDirection(0);
    trackB->baseHeight = 32;

    // Set station start at Piece A
    ride->getStation(StationIndex::FromUnderlying(0)).Start = CoordsXY(32 * kCoordsXYStep, 32 * kCoordsXYStep);

    // This should NOT crash/hang
    // In the buggy version, it will infinite loop here
    ride->validateStations();

    SUCCEED();
}
