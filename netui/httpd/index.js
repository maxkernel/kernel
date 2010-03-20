var titlebar = new Ext.Panel({
	region: 'north',
	border: false,
	html: '<div id="header"><h1>Max Dashboard</h1></div>',
});

var contentpane = new Ext.Panel({
	region: 'center',
	border: false,
	margins: '5 5 5 0',
	layout: 'card',
	activeItem: 0,
	items: [p_dashboard, Cal.panel, p_system, p_deploy, p_env]
});

var menupane = new Ext.tree.TreePanel({
	title: 'Navigation',
	split: true,
	collapsible: true,
	region: 'west',
	width: 250,
	minSize: 100,
	maxSize: 400,
	margins: '5 0 5 5',
	loader: new Ext.tree.TreeLoader(),
	rootVisible: false,
	lines: false,
	autoScroll: true,
	root: new Ext.tree.AsyncTreeNode({
		id: 'nav',
		expanded: true,
		children: [{
			text: 'General',
			id: 'general',
			cls: 'menu-rt',
			expanded: true,
			children: [{
				text: 'Dashboard',
				id: 'dashboard',
				leaf: true,
				listeners: {click: function(n) { showpage(n.id); }}
			},{
				text: 'Calibration',
				id: 'calibrate',
				leaf: true,
				icon: '/images/gears.png',
				listeners: {click: function(n) { showpage(n.id); }}
			}]
		},{
			text: 'Documentation',
			id: 'doc',
			cls: 'menu-rt',
			expanded: true,
			children: [{
				text: 'System',
				id: 'system',
				leaf: true,
				listeners: {click: function(n) { showpage(n.id); }}
			},{
				text: 'Deployment',
				id: 'deploy',
				leaf: true,
				listeners: {click: function(n) { showpage(n.id); }}
			},{
				text: 'Environment',
				id: 'env',
				leaf: true,
				listeners: {click: function(n) { showpage(n.id); }}
			}]
		}]
	}),
	listeners: {
		load: function(n) {
			var i = menupane.getNodeById('dashboard');
			if (i != undefined)
			{
				i.select();
			}
		}
	}
});
