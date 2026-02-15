registerPlugin({
    name: 'Async Test Plugin',
    version: '1.0',
    authors: ['OpenRCT2 Developers'],
    type: 'local',
    licence: 'GPL-v3',
    minApiVersion: 110,
    targetApiVersion: 110,
    main: function() {
        var timeoutFired = false;
        context.setTimeout(function() {
            timeoutFired = true;
            console.log("Timeout fired!");
        }, 100);

        // We will check timeoutFired from C++ after advancing time
        context.registerAction("checkTimeout", function() { return {}; }, function() {
            test.assert(timeoutFired, "Timeout should have fired by now");
            return {};
        });
    }
});
