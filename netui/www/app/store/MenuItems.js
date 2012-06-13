Ext.define('Max.store.MenuItems', {
    extend: 'Ext.data.Store',
    model: 'Max.model.MenuItem',

    data: [
    	{name: 'Dashboard', icon: 'dashboard.png', path: 'Max.controller.Dashboard', view: 'dashboard'},
        {name: 'Calibration', icon: 'calibration.png', path: 'Max.controller.Calibration', view: 'calibration'},
        {name: 'Objects', icon: 'objects.png', path: 'Max.controller.Objects', view: 'objects'}
    ]
});

