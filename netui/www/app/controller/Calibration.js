Ext.define('Max.controller.Calibration', {
    extend: 'Ext.app.Controller',

    models: ['CalibrationItem'],
    stores: ['CalibrationItems'],
    
    requires: [
        'Max.view.calibration.List'
    ],
    
    refs: [
        {ref: 'viewer', selector: 'viewer'}
    ],

    init: function() {
        this.getViewer().showTab({title: 'Calibration', closable: true, xtype: 'calibrationlist'});
        
        this.control({
            'calibrationlist button[action=commit]': {
                click: this.commit
            },
            
            'calibrationlist button[action=revert]': {
                click: this.revert
            },
        });
    },
    
    commit: function(button) {
        alert('TODO - incomplete');
    },
    
    revert: function(button) {
        alert('TODO - incomplete');
    },
});
