Ext.define('Max.store.ObjectsItems', {
    extend: 'Ext.data.Store',
    model: 'Max.model.ObjectsItem',
    
    groupField: 'name',
    
    proxy: {
        type: 'ajax',
        url: '/get/objects.json',
        
        reader: {
            type: 'json',
            root: 'result'
        },
        
        filterParam: undefined,
        groupParam: undefined,
        pageParam: undefined,
        startParam: undefined,
        sortParam: undefined,
        limitParam: undefined,
    },
    
    autoLoad: true,
});

