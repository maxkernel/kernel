var Cal = {
	loading: false,
	loaded: false,
	rownum: 0,
	
	setparam: function(mod, name, value)
	{
		if (!this.loading)
		{
			new Ext.data.ScriptTagProxy({url: '/calset'}).request('read', null, {'module':mod,'name':name,'value': value}, {}, function(){}, null, {});
		}
	},
	
	reset: function(revert)
	{
		this.loaded = true;
		var args = {};
		if (revert)
		{
			args = {'revert': '1'};
		}
		
		new Ext.data.ScriptTagProxy({url: '/calget'}).request('read', null, args, {}, function(){}, null, {});
	},
	
	save: function()
	{
		Ext.MessageBox.show({
			title: 'Please wait',
			msg: 'Updating your calibration changes...',
			progressText: 'Uploading...',
			width: 300,
			wait: true,
			waitConfig: {interval: 100},
			progress: true,
		});

		setTimeout(function(){if (Ext.MessageBox.isVisible()){Ext.MessageBox.hide();Ext.MessageBox.alert('Error','Upload calibration failed')}}, 1500);
		
		var comment = Ext.get('cal_comment').getValue();
		if (comment == 'comment')
			comment = '';
		
		new Ext.data.ScriptTagProxy({url: '/calsave'}).request('read', null, {'comment':comment}, {}, function(){}, null, {});
	},
	
	
	mklabel: function(name)
	{
		var new_name = '';
		name = name.replace(/_/, ' ');
		name = name.split(' ');
		for(var c=0; c < name.length; c++) {
			new_name += name[c].substring(0,1).toUpperCase() + name[c].substring(1,name[c].length) + ' ';
		}
		
		var c = {
			xtype: 'label',
			style: 'padding-left: 10px; font-family: sans-serif; font-size: 10pt; font-weight: bold;',
			width: 150,
			text: new_name,
		};
		return c;
	},
	mkspinner: function(mod, name, type, val, min, max, step, bg)
	{
		var c = new Ext.Panel({
			width: 90,
			border: false,
			bodyStyle: 'background: '+bg,
			items: [{
				xtype: 'spinnerfield',
				value: val,
				width: 75,
				minValue: min,
				maxValue: max,
				allowDecimals: (type == 'd')? true : false,
				incrementValue: step,
				alternateIncrementValue: step*10,
				accelerate: true,
				listeners: {
					valid: function(o){Cal.setparam(mod, name, o.getValue());},
					spin: function(o){Cal.setparam(mod, name, o.field.getValue());}
				}
			}]
		});
		return c;
	},
	mkdesc: function(desc)
	{
		var c = {
			xtype: 'label',
			columnWidth: 1,
			style: 'padding-left: 10px; font-family: sans-serif; font-size: 10pt;',
			text: desc,
		};
		return c;
	},
	mkrow: function(mod, name, type, desc, val, min, max, step)
	{
		var b = (this.rownum++ % 2 == 0)? '#fff' : '#f5f5f5';
		var c = {
			layout: 'column',
			border: false,
			id: 'cal_row_'+name,
			bodyStyle: 'padding: 5px; background: '+b,
			items: [
				Cal.mklabel(name),
				Cal.mkspinner(mod, name, type, val, min, max, step, b),
				Cal.mkdesc(desc),
			],
			listeners: {
				render: function() {Ext.getCmp('cal_row_'+name).body.slideIn('t');Ext.getCmp('cal_row_'+name).body.fadeIn();}
			}
		};
	
		return c;
	},
};

Cal.calTab = new Ext.Panel({
	title: 'Calibration',
	autoScroll: true,
	items: [
		{ xtype: 'label', text: 'Loading...' },
	],
	tbar: [
		' ',
		new Ext.form.TextField({ xtype: 'textfield', id: 'cal_comment', width: 250, emptyText: 'comment'}),
		{ text: 'Save', icon: 'images/save.png', height: 25, width: 65, handler: Cal.save },
		'-',
		{ text: 'Cancel', icon: 'images/cancel.png', height: 25, width: 65, handler: function(){Cal.reset(true);} }
	],
});

Cal.historyTab = new Ext.Panel({
	title: 'History',
	autoscroll: true,
	layout: 'hbox',
	layoutConfig: {align : 'stretch'},
	items: [grid],
	tbar: [
		{ text: 'Restore', icon: 'images/restore.png', height: 25, width: 65, handler: function() { console.log(grid.getSelectionModel().getSelections())} }
	],
	listeners: {
		show: function() {console.log('S');},
		hide: function() {console.log('H');}
	}
});

Cal.panel = new Ext.TabPanel({
	activeTab: 0,
	items: [Cal.calTab, Cal.historyTab],
	listeners: {
		show: function() {if (!Cal.loaded){Cal.reset(false)}}
	}
});

