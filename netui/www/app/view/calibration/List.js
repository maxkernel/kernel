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
				    text: 'Start',
				    name: 'start',
				    icon: 'images/cal-start.png',
				    action: 'start'
				},{
					text: 'Revert',
					name: 'revert',
					disabled: true,
					icon: 'images/cancel.png',
					action: 'revert'
				},{
					xtype: 'tbspacer',
					flex: 1
				},{
				    xtype: 'textfield',
                    name: 'comment',
                    disabled: true,
                    emptyText: 'enter calibration comment...',
                    width: 300
				},{
					text: 'Commit',
					name: 'commit',
					disabled: true,
					icon: 'images/save.png',
					action: 'commit'
				}]
			}]
		});

		this.callParent(arguments);
		this.fireEvent("init", null);
	},
	
	loadStore: function() {
		this.getStore().load();
	},
	clearStore: function() {
		this.getStore().removeAll();
	},
	
	formatValue: function(value, metaData, record) {
        var self = this;
	    var id = Ext.id();
	    
	    Ext.Function.defer(function() {
	        var c = Ext.create('Ext.form.NumberField', {
	            renderTo: id,
	            height: 17,
	            value: value,
	            step: record.data.step,
	            action: 'preview',
	            tip: Ext.widget('tooltip', {
                    target: id,
                    title: 'Constraints',
                    disabled: true,
                    autoHide: false,
                    anchor: 'right',
                    constrainPosition: false,
                    width: 150
                }),
	            
	            listeners: {
	                change: function(evt) { self.fireChange(evt, c.tip, record); }
	            }
	        });
	    }, 50);
	    
	    return Ext.String.format('<div id="{0}"></div>', id);
	},
	
	fireChange: function(evt, tip, record) {
	    evt['record'] = record;
	    evt['tip'] = tip;
	    this.fireEvent("preview", evt);
	},
});

