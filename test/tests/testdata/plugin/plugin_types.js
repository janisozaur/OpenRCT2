registerPlugin({
    name: 'Plugin Types Test',
    version: '1.0',
    authors: ['OpenRCT2 Developers'],
    type: 'intransient',
    licence: 'GPL-v3',
    minApiVersion: 110,
    targetApiVersion: 110,
    main: function() {
        console.log("Intransient plugin started");
        context.registerAction("checkType", function() { return {}; }, function() {
            return { cost: 42 };
        });
    }
});
