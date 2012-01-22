Ext.define('Max.store.MenuItems', {
    extend: 'Ext.data.Store',
    model: 'Max.model.MenuItem',

    data: [
        {name: 'Calibration', icon: 'calibration.png', path: 'Max.controller.Calibration'}
    ]
});

