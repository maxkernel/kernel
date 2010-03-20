var pages = {
	'dashboard': 0,
	'calibrate': 1,
	'system': 2,
	'deploy': 3,
	'env': 4
}


var p_dashboard = new Ext.Panel({
	border: false,
	html: '<h1>Dashboard</h1>',
});



var p_system = new Ext.Panel({
	border: false,
	html: '<h1>System</h1>',
});

var p_deploy = new Ext.Panel({
	border: false,
	html: '<h1>Deployment</h1>',
});

var p_env = new Ext.Panel({
	border: false,
	html: '<h1>Environment</h1>',
});



var xg = Ext.grid;

// Array data for the grids
Ext.grid.dummyData = [
	['foo', '1.0', '10/10/1000', 'comment', 'module'],
	['bar', '2.5', '05/05/2005', 'comment2', 'module2'],
];

// shared reader
var reader = new Ext.data.ArrayReader({}, [
	{name: 'name'},
	{name: 'value', type: 'float'},
	{name: 'updated', type: 'date', dateFormat: 'm/d/Y'},
	{name: 'comment'},
	{name: 'module' },
]);

var grid = new xg.GridPanel({
	border: false,
	width: 600,
	store: new Ext.data.GroupingStore({
		reader: reader,
		data: xg.dummyData,
		sortInfo: {field: 'name', direction: 'ASC'},
		groupField: 'updated'
	}),

	columns: [
		{header: "Name", width: 40, sortable: true, dataIndex: 'name'},
		{header: "Value", width: 20, sortable: false, dataIndex: 'value'},
		{header: "Updated", width: 30, sortable: true, renderer: Ext.util.Format.dateRenderer('F j, Y'), dataIndex: 'updated'},
		{header: "Comment", width: 60, sortable: false, dataIndex: 'comment'},
	],

	view: new Ext.grid.GroupingView({
		forceFit: true,
	}),
	
	
});


