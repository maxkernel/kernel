Ext.define('Max.view.calibration.List', {
	extend: 'Ext.grid.Panel',
	alias: 'widget.calibrationlist',
	
    requires: ['Ext.toolbar.Toolbar'],
    store: 'CalibrationItems',

	initComponent: function() {
	    var grouping = Ext.create('Ext.grid.feature.Grouping', {
            groupHeaderTpl: '{name}'
        });
        
		Ext.apply(this, {
			features: [grouping],
			
			cellCls: 'valign-middle',
			
			columns: [{
				text: 'Name',
				dataIndex: 'name',
				width: 200
			}, {
				text: 'Description',
				dataIndex: 'desc',
				flex: 1
			}, {
				text: 'Value',
				dataIndex: 'value',
				width: 200,
				renderer: this.formatValue
			}],

			dockedItems: [{
				dock: 'top',
				xtype: 'toolbar',
				items: [{
				    xtype: 'textfield',
                    name: 'comment',
                    emptyText: 'enter calibration comment...',
                    width: 300
				}, {
					text: 'Commit',
					icon: 'images/save.png',
					action: 'commit'
				},{
					text: 'Revert',
					icon: 'images/cancel.png',
					action: 'revert'
				}]
			}]
		});

		this.callParent(arguments);
	},
	
	formatValue: function(value, metaData, record) {
        var self = this;
	    var id = Ext.id();
	    
	    Ext.Function.defer(function() {
	        var c = Ext.create('Ext.form.NumberField', {
	            renderTo: id,
	            height: 17,
	            value: value,
	            minValue: record.data.min,
	            maxValue: record.data.max,
	            step: self.getStep(Math.abs(record.data.max - record.data.min)),
	            defaultValue: value,
	            action: 'preview',
	            
	            listeners: {
	                change: function(evt) { self.fireChange(evt, record); }
	            }
	        });
	        
	        self.addListener("revert", function() {
	            c.setValue(c.defaultValue);
	        });
	        self.addListener("commit", function() {
	            c.defaultValue = c.getValue();
	        });
	    }, 50);
	    
	    return Ext.String.format('<div id="{0}"></div>', id);
	},
	
	fireChange: function(evt, record) {
	    evt['record'] = record;
	    this.fireEvent("preview", evt);
	},
	
	getStep: function(stride) {
	    if (stride <= 10.0)             return 0.1;
	    else if (stride <= 100.0)		return 1.0;
	    else if (stride <= 1000.0)		return 10.0;
	    else if (stride <= 10000.0)		return 100.0;
	    else							return 1000.0;
	}
});

