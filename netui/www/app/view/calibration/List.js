Ext.define('Max.view.calibration.List', {
	extend: 'Ext.grid.Panel',
	alias: 'widget.calibrationlist',
	
    requires: ['Ext.toolbar.Toolbar'],
    store: 'CalibrationItems',

	//autoScroll: true,
	
	initComponent: function() {
	    var grouping = Ext.create('Ext.grid.feature.Grouping', {
            groupHeaderTpl: '{name}'
        });
    
		Ext.apply(this, {
			/*tpl: new Ext.XTemplate(
			    '<div class="post-data">',
			        '<span class="post-date">{pubDate:this.formatDate}</span>',
			        '<h3 class="post-title">{title}</h3>',
			        '<h4 class="post-author">by {author:this.defaultValue}</h4>',
			    '</div>',
			    '<div class="post-body">{content:this.getBody}</div>', {

				getBody: function(value, all) {
					return Ext.util.Format.stripScripts(value);
				},

				defaultValue: function(v) {
					return v ? v : 'Unknown';
				},

				formatDate: function(value) {
					if (!value) {
						return '';
					}
					return Ext.Date.format(value, 'M j, Y, g:i a');
				}
			}),*/
			
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
	    var id = Ext.id();
	    
	    console.log(record);
	    
	    Ext.Function.defer(function() {
	        Ext.create('Ext.form.NumberField', {
	            renderTo: id,
	            height: 17,
	            value: value
	        });
	    }, 25);
	    
	    return Ext.String.format('<div id="{0}"></div>', id);
	}
});

