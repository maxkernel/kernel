Ext.define('Max.controller.Calibration', {
    extend: 'Ext.app.Controller',

    models: ['CalibrationItem'],
    stores: ['CalibrationItems'],
    
    requires: ['Max.view.Calibration'],
    refs: [{ref: 'viewer', selector: 'viewer'}],

    init: function() {
        this.control({
	        'calibration *[action=start]': { click: this.start },
            'calibration *[action=commit]': { click: this.commit },
            'calibration *[action=revert]': { click: this.revert },
            'calibration': { preview: this.preview, init: this.initview }
        });
    },
    
    setmode: function(incal) {
    	this.getViewer().getTab('Calibration').down('[name=start]').setDisabled(incal);
    	this.getViewer().getTab('Calibration').down('[name=revert]').setDisabled(!incal);
    	this.getViewer().getTab('Calibration').down('[name=comment]').setDisabled(!incal);
    	this.getViewer().getTab('Calibration').down('[name=commit]').setDisabled(!incal);
    },
    initview: function(evt) {
        var self = this;
		this.getViewer().setLoading(true);
        Ext.Ajax.request({
		    url: '/get/calibration/mode',
		    method: 'GET',
		    success: function(response) {
		        self.getViewer().setLoading(false);
		        var lines = response.responseText.split("\n");
		        if (response.responseText.toLowerCase().indexOf("ok") != 0)
		        {
		            self.showError('Server failure.<br />'+response.responseText);
		        }
		        else if (lines[1].toLowerCase() == "calibrating")
		        {
		        	self.setmode(true);
				    self.getViewer().getTab('Calibration').loadStore();
					
    			}
    			else
    			{
    				self.getViewer().getTab('Calibration').clearStore();
    				self.setmode(false);
    			}
		    },
		    failure: function(response) {
		        self.getViewer().setLoading(false);
		        self.showError('Could not connect to server.');
		    }
		});
    },
    start: function(evt) {
		var self = this;
		this.getViewer().setLoading(true);
		
		Ext.Ajax.request({
		    url: '/set/calibration/start',
		    method: 'GET',
		    params: {
		        
		    },
		    success: function(response) {
		        self.getViewer().setLoading(false);
		        if (response.responseText.toLowerCase() != "ok")
		        {
		            self.showError('Server failure.<br />'+response.responseText);
		        }
		        
		        self.getViewer().getTab('Calibration').loadStore();
    			self.setmode(true);
		    },
		    failure: function(response) {
		        self.getViewer().setLoading(false);
		        self.showError('Could not connect to server.');
		    }
		});
    },
    preview: function(evt) {
        var self = this;
        
        Ext.Ajax.request({
            url: '/set/calibration/preview',
            method: 'GET',
            params: {
            	domain: evt.record.data.name,
                name: evt.record.data.subname,
                value: evt.value
            },
            success: function(response) {
            	var lines = response.responseText.split("\n");
                if (response.responseText.toLowerCase().indexOf("ok") != 0)
                {
                    self.showError('Server failure.<br />'+response.responseText);
                }
                else
                {
                	Ext.getCmp(evt.id).setRawValue(lines[1]);
                	
                	if (lines[2] != "")
                	{
		            	evt.tip.enable();
		            	evt.tip.show();
		                evt.tip.update(lines[2]);
		            }
		            else
		            {
		            	evt.tip.disable();
		            }
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
                
                self.getViewer().getTab('Calibration').clearStore();
                self.setmode(false);
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
        
        Ext.Ajax.request({
            url: '/set/calibration/revert',
            method: 'GET',
            success: function(response) {
                self.getViewer().setLoading(false);
                if (response.responseText.toLowerCase() != "ok")
                {
                    self.showError('Server failure.<br />'+response.responseText);
                }
                
                self.getViewer().getTab('Calibration').clearStore();
                self.setmode(false);
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
