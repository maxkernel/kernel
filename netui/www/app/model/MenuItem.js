Ext.define('Max.model.MenuItem', {
    extend: 'Ext.data.Model',
    
    fields: [
        {name: 'name', type: 'string'},
        {name: 'icon', type: 'string'},
        {name: 'path', type: 'string'},
        {name: 'view', type: 'string'}
    ]
});
