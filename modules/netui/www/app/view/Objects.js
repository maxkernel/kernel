Ext.define('Max.view.Objects', {
	extend: 'Ext.panel.Panel',
	alias: 'widget.objects',
	
	initComponent: function() {
		var self = this;
		Ext.apply(this, {
			layout: 'border',
			items: [{
				name: 'objects_grid',
				region: 'center',
				flex: 1,
				xtype: 'grid',
				store: 'ObjectsItems',
				
				features: [{
					ftype: 'grouping',
					groupHeaderTpl: 'Class: {name}'
				}],

				columns: [{
					text: 'Name',
					dataIndex: 'subname',
					flex: 1
				}, {
					text: 'ID',
					dataIndex: 'id',
					width: 75
				}, {
					text: 'Parent',
					dataIndex: 'parent',
					width: 75
				}]
			},{
				title: 'Object Inspector',
				name: 'objects_inspector',
				region: 'east',
				split: true,
				flex: 1,
				xtype: 'propertygrid',
				
				source: {
					'': ''
				},
				
				listeners: {
					'propertychange': {
						fn: function(source, recordId, value, oldValue, eOpts){
							source[recordId] = oldValue;
							self.down('[name=objects_inspector]').setSource(source);
						}
					}
				}
			}]
		});

		self.callParent(arguments);
		self.down('[name=objects_grid]').getSelectionModel().on('selectionchange', function(model, selected, eOpts) {
			var json = Ext.decode(selected[0].data.desc, true);
			if (json == null)
			{
				json = {
					'error': 'Parse error',
					'data': selected[0].data.desc
				}
			}
			
			self.down('[name=objects_inspector]').setSource(json);
		});
	},
});

