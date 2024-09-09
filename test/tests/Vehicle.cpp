#include <gtest/gtest.h>
#include <openrct2/ride/Track.h>
#include <openrct2/ride/Vehicle.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/object/RideObject.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/Context.h>
#include <openrct2/object/ObjectManager.h>

using namespace OpenRCT2;

class VehicleTest : public testing::Test
{
protected:
    Vehicle vehicle;
    CarEntry carEntry;
    Ride ride;
    RideObjectEntry rideEntry;

    void SetUp() override
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        const bool initialised = _context->Initialise();
        // Load tigrtwst as a legacy object
        auto& obj_manager = _context->GetObjectManager();
        auto object = obj_manager.LoadObject("rct2.ride.cboat");
        ASSERT_NE(nullptr, object);
        ASSERT_TRUE(initialised);

        vehicle = Vehicle();
        carEntry = CarEntry();
        ride = Ride();
        rideEntry = RideObjectEntry();
    }

    static void TearDownTestCase()
    {
        _context = nullptr;
    }

private:
    static std::shared_ptr<IContext> _context;
};

std::shared_ptr<IContext> VehicleTest::_context;

TEST_F(VehicleTest, UpdateTrackMotionForwards_HeartLineTransfer)
{
    vehicle.SetTrackType(TrackElemType::HeartLineTransferUp);
    vehicle.track_progress = 80;
    vehicle.vehicle_type = 0;
    vehicle.velocity = 0x50000;

    EXPECT_TRUE(vehicle.UpdateTrackMotionForwards(&carEntry, ride, rideEntry));
    EXPECT_EQ(vehicle.vehicle_type, 1);
    EXPECT_EQ(vehicle.acceleration, 0x50000);
}

// TEST_F(VehicleTest, UpdateTrackMotionForwards_Brakes)
// {
//     vehicle.SetTrackType(TrackElemType::Brakes);
//     vehicle.velocity = 0x60000;
//     ride.lifecycle_flags = 0;
//     ride.breakdown_reason_pending = 0;

//     EXPECT_TRUE(vehicle.UpdateTrackMotionForwards(&carEntry, ride, rideEntry));
//     EXPECT_LT(vehicle.acceleration, 0);
// }

// TEST_F(VehicleTest, UpdateTrackMotionForwards_Booster)
// {
//     vehicle.SetTrackType(TrackElemType::Booster);
//     vehicle.velocity = 0x20000;
//     ride.type = RIDE_TYPE_LOOPING_ROLLER_COASTER;

//     EXPECT_TRUE(vehicle.UpdateTrackMotionForwards(&carEntry, ride, rideEntry));
//     EXPECT_GT(vehicle.acceleration, 0);
// }

// TEST_F(VehicleTest, UpdateTrackMotionForwards_BrakeForDrop)
// {
//     vehicle.SetTrackType(TrackElemType::BrakeForDrop);
//     vehicle.SetFlag(VehicleFlags::IsHead);
//     vehicle.track_progress = 25;

//     EXPECT_TRUE(vehicle.UpdateTrackMotionForwards(&carEntry, ride, rideEntry));
//     EXPECT_TRUE(vehicle.HasFlag(VehicleFlags::StoppedOnHoldingBrake));
//     EXPECT_EQ(vehicle.vertical_drop_countdown, 90);
// }

// TEST_F(VehicleTest, UpdateTrackMotionForwards_LogFlumeReverser)
// {
//     vehicle.SetTrackType(TrackElemType::LogFlumeReverser);
//     vehicle.track_progress = 16;
//     vehicle.velocity = 5.0_mph;

//     EXPECT_TRUE(vehicle.UpdateTrackMotionForwards(&carEntry, ride, rideEntry));
//     EXPECT_EQ(vehicle.track_progress, 33);
// }

// TEST_F(VehicleTest, UpdateTrackMotionForwards_EndOfTrack)
// {
//     vehicle.SetTrackType(TrackElemType::Flat);
//     vehicle.track_progress = 100;
//     vehicle.remaining_distance = 10;

//     EXPECT_FALSE(vehicle.UpdateTrackMotionForwards(&carEntry, ride, rideEntry));
//     EXPECT_EQ(vehicle.remaining_distance, -1);
// }
