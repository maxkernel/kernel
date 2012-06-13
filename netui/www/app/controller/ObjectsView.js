Ext.define('Max.controller.ObjectsView', {
    extend: 'Ext.app.Controller',

    models: ['ObjectsItem'],
    stores: ['ObjectsItems'],
    
    requires: [
        'Max.view.objects.List'
    ],
    
    refs: [
        {ref: 'viewer', selector: 'viewer'}
    ],

    init: function() {
        this.control({
            'objectslist': { init: this.initview }
        });
    },
    
	initview: function(evt) {
        var self = this;
		self.getViewer().getTab('Objects').loadStore();
    },
});
