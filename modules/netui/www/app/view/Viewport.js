Ext.define('Max.view.Viewport', {
    extend: 'Ext.container.Viewport',

    requires: [
        'Max.view.Viewer',
        'Max.view.Menu',
        'Ext.layout.container.Border'
    ],

	layout: 'border',
	
	items: [{
		xtype: 'box',
		id: 'banner',
		region: 'north',
		html: '<h1>MaxKernel Dashboard</h1>',
		height: 30
	},{
		region: 'center',
		xtype: 'viewer'
	},
	{
		region: 'west',
		width: 225,
		xtype: 'menu'
	}]
});

