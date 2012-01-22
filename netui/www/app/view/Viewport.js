Ext.define('Max.view.Viewport', {
    extend: 'Ext.container.Viewport',

    requires: [
        'Max.view.Viewer',
        'Max.view.Menu',
        'Max.view.Banner',
        'Ext.layout.container.Border'
    ],

	layout: 'border',
	
	items: [{
	    region: 'north',
	    xtype: 'banner'
	},
	{
		region: 'center',
		xtype: 'viewer'
	},
	{
		region: 'west',
		width: 225,
		xtype: 'menu'
	}]
});

