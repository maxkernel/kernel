Ext.define('Max.model.CalibrationItem', {
    extend: 'Ext.data.Model',
    
    fields: [
        {name: 'group', type: 'string'},
        {name: 'name',  type: 'string'},
        {name: 'type', type: 'string'},
        {name: 'value',  type: 'string'},
        {name: 'min', type: 'string'},
        {name: 'max', type: 'string'},
        {name: 'desc',  type: 'string'}
    ]
});

