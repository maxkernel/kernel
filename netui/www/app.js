Ext.Loader.setConfig({ 
    enabled: true,
});

Ext.onReady(function() {
	Ext.create('Ext.app.Application', {
		name: 'Max',
    	controllers: ['Menu'],
    	autoCreateViewport: true,
    	
		launch: function() {
			//var self = this;
			this.controllers.addListener('add', function(index, controller, key) {
				controller.init();
			});
		},
	});
});

