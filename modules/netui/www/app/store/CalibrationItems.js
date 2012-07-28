Ext.define('Max.store.CalibrationItems', {
    extend: 'Ext.data.Store',
    model: 'Max.model.CalibrationItem',
    
    groupField: 'name',
    
    proxy: {
        type: 'ajax',
        url: '/get/calibration.json',
        
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
});

