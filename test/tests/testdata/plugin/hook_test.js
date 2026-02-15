registerPlugin({
    name: 'Hook Test Plugin',
    version: '1.0',
    authors: ['OpenRCT2 Developers'],
    type: 'local',
    licence: 'GPL-v3',
    minApiVersion: 110,
    targetApiVersion: 110,
    main: function() {
        var actionExecuted = false;
        context.subscribe('action.execute', function(e) {
            if (e.action === 'parksetname') {
                actionExecuted = true;
                test.assert(e.args.name === 'Hooked Park', "Hooked action name mismatch");
            }
        });

        context.executeAction('parksetname', { name: 'Hooked Park' }, function(result) {
            test.assert(actionExecuted, "Action execute hook should have fired");
        });
    }
});
