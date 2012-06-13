Ext.define('Max.controller.Objects', {
    extend: 'Ext.app.Controller',

    models: ['ObjectsItem'],
    stores: ['ObjectsItems'],
    
    requires: [
        'Max.view.Objects'
    ]
});

