Ext.define('Max.view.Menu', {
	extend: 'Ext.panel.Panel',
	alias: 'widget.menu',

	title: 'Menu',
	collapsible: true,
	animCollapse: true,
	margins: '5 0 5 5',
	layout: 'fit',

	initComponent: function() {
		Ext.apply(this, {
			items: [{
				xtype: 'dataview',
				trackOver: true,
				store: this.store,
				cls: 'menu-list',
				itemSelector: '.menu-list-item',
				overItemCls: 'menu-list-item-hover',
				tpl: '<tpl for="."><div class="menu-list-item"><div style="background: url(images/{icon}) no-repeat 0 2px; padding-left: 20px;">{name}</div></div></tpl>',
			}],
		});

		this.callParent(arguments);
	},
});

