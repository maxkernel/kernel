Ext.define('Max.model.ObjectsItem', {
    extend: 'Ext.data.Model',
    
    fields: [
        {name: 'name', type: 'string'},
        {name: 'subname',  type: 'string'},
        {name: 'id', type: 'string'},
        {name: 'parent', type: 'string'},
        {name: 'desc',  type: 'string'}
    ]
});

