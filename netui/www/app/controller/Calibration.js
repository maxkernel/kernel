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
        this.getViewer().showTab({title: 'Calibration', xtype: 'calibrationlist'});
        
        this.control({
            'calibrationlist *[action=commit]': { click: this.commit },
            'calibrationlist *[action=revert]': { click: this.revert },
            'calibrationlist': { preview: this.preview }
        });
    },
    
    preview: function(evt) {
        var self = this;
        
        Ext.Ajax.request({
            url: '/set/calibration/preview',
            method: 'GET',
            params: {
                module: evt.record.data.module,
                name: evt.record.data.name,
                value: evt.value
            },
            success: function(response) {
                if (response.responseText.toLowerCase() != "ok")
                {
                    self.showError('Server failure.<br />'+response.responseText);
                }
            },
            failure: function(response) {
                self.showError('Could not connect to server.');
            }
        });
    },
    commit: function(evt) {
        var self = this;
        var comment = this.getViewer().getTab('Calibration').down('[name=comment]').value
        this.getViewer().setLoading(true);
        this.getViewer().getTab('Calibration').fireEvent("commit");
        
        Ext.Ajax.request({
            url: '/set/calibration/commit',
            method: 'GET',
            params: {
                comment: comment
            },
            success: function(response) {
                self.getViewer().setLoading(false);
                if (response.responseText.toLowerCase() != "ok")
                {
                    self.showError('Server failure.<br />'+response.responseText);
                }
                else
                {
                    self.showMessage('Successful commit with comment:<br />'+comment);
                }
            },
            failure: function(response) {
                self.getViewer().setLoading(false);
                self.showError('Could not connect to server.');
            }
        });
    },
    revert: function(evt) {
        var self = this;
        this.getViewer().setLoading(true);
        this.getViewer().getTab('Calibration').fireEvent("revert");
        
        Ext.Ajax.request({
            url: '/set/calibration/revert',
            method: 'GET',
            params: {
                
            },
            success: function(response) {
                self.getViewer().setLoading(false);
                if (response.responseText.toLowerCase() != "ok")
                {
                    self.showError('Server failure.<br />'+response.responseText);
                }
                else
                {
                    self.showMessage('Reverted calibration.');
                }
            },
            failure: function(response) {
                self.getViewer().setLoading(false);
                self.showError('Could not connect to server.');
            }
        });
    },
    
    
    showMessage: function(msg) {
        Ext.Msg.show({
            title:'Message',
            msg: msg,
            buttons: Ext.Msg.OK,
            icon: Ext.Msg.INFO
        });
    },
    showError: function(msg) {
        Ext.Msg.show({
            title:'Error!',
            msg: msg,
            buttons: Ext.Msg.OK,
            icon: Ext.Msg.ERROR
        });
    }
});
