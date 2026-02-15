registerPlugin({
    name: 'Action Test Plugin',
    version: '1.0',
    authors: ['OpenRCT2 Developers'],
    type: 'remote',
    licence: 'GPL-v3',
    minApiVersion: 110,
    targetApiVersion: 110,
    main: function() {
        console.log("Action test plugin started");
        context.executeAction('parksetname', { name: 'Test Park' }, function(result) {
            test.assert(result.error === 0, "Set park name action failed");
            test.assert(park.name === 'Test Park', "Park name should be 'Test Park'");
        });

        context.executeAction('parksetentrancefee', { value: 123 }, function(result) {
            test.assert(result.error === 0, "Set entrance fee action failed");
            test.assert(park.entranceFee === 123, "Entrance fee should be 123");
        });
    }
});
