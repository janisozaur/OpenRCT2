registerPlugin({
    name: 'Ride Construction Test',
    version: '1.0',
    authors: ['OpenRCT2 Developers'],
    type: 'remote',
    licence: 'GPL-v3',
    minApiVersion: 110,
    targetApiVersion: 110,
    main: function() {
        console.log("Ride construction plugin started");

        var rides = objectManager.getAllObjects('ride');
        var rideObj = rides.filter(function(r) { return r.identifier === 'rct2.ride.mgr1'; })[0];
        if (!rideObj) rideObj = rides[0];

        var rideType = rideObj.rideType[0];
        console.log("Using ride object: " + rideObj.identifier + " (rideType: " + rideType + ", index: " + rideObj.index + ")");

        context.executeAction('ridecreate', {
            rideType: rideType,
            rideObject: rideObj.index,
            colour1: 0,
            colour2: 0,
            entranceObject: 0,
            inspectionInterval: 1
        }, function(result) {
            console.log("ridecreate result: " + result.error + ", ride: " + result.ride);
            test.assert(result.error === 0, "Ride creation failed");
            var rideId = result.ride;

            var x = 320;
            var y = 320;
            var z = 16;

            // Carousel piece
            context.executeAction('trackplace', {
                ride: rideId,
                trackType: 0,
                x: x, y: y, z: z,
                direction: 0,
                rideType: rideType,
                brakeSpeed: 0,
                colour: 0,
                seatRotation: 0,
                trackPlaceFlags: 0,
                isFromTrackDesign: false
            }, function(res1) {
                console.log("trackplace result: " + res1.error);
                test.assert(res1.error === 0, "Track place failed");

                // Set operating mode to "Single Ride per Admission" (Mode 9)
                // Just to test that we can change it from the default (10)
                context.executeAction('ridesetsetting', {
                    ride: rideId,
                    setting: 0, // Mode
                    value: 9    // Single Ride
                }, function(resM) {
                    console.log("ridesetsetting result: " + resM.error);
                    test.assert(resM.error === 0, "Set operating mode failed");
                    console.log("Ride construction complete");
                });
            });
        });
    }
});
