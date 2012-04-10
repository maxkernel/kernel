Ext.define('Max.model.CalibrationItem', {
    extend: 'Ext.data.Model',
    
    fields: [
        {name: 'domain', type: 'string'},
        {name: 'name',  type: 'string'},
        {name: 'sig', type: 'string'},
        {name: 'value',  type: 'string'},
        {name: 'step', type: 'number'},
        {name: 'desc',  type: 'string'}
    ]
});

