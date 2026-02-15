registerPlugin({
    name: 'Basic Test Plugin',
    version: '1.0',
    authors: ['OpenRCT2 Developers'],
    type: 'local',
    licence: 'GPL-v3',
    minApiVersion: 110,
    targetApiVersion: 110,
    main: function() {
        test.assert(park.name !== undefined, "Park name should be defined");
        console.log("Basic test plugin started");
    }
});
