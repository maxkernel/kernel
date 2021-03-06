Ext.define('Max.view.Calibration', {
	extend: 'Ext.grid.Panel',
	alias: 'widget.calibration',
	
    requires: ['Ext.toolbar.Toolbar'],
    store: 'CalibrationItems',

	initComponent: function() {
		Ext.apply(this, {
			features: [{
				ftype: 'grouping',
				groupHeaderTpl: 'Domain: {name}'
			}],
			
			cellCls: 'valign-middle',
			
			columns: [{
				text: 'Name',
				dataIndex: 'subname',
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
				    icon: 'images/calibration-start.png',
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
		this.features[0].enable();
		this.getStore().load();
	},
	clearStore: function() {
		this.getStore().removeAll();
		this.features[0].disable();
		this.getStore().add({desc: '<i>Press Start to begin calibration</i>'});
	},
	
	formatValue: function(value, metaData, record) {
		if (value == '')
		{
			return '';
		}
		
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

