Ext.define('Max.store.MenuItems', {
    extend: 'Ext.data.Store',
    model: 'Max.model.MenuItem',

    data: [
        {name: 'Calibration', icon: 'calibration.png', path: 'Max.controller.Calibration', view: 'calibrationlist'},
        {name: 'Objects', icon: 'objects.png', path: 'Max.controller.ObjectsView', view: 'objectslist'}
    ]
});

