Ext.define('Max.controller.Menu', {
    extend: 'Ext.app.Controller',

    stores: ['MenuItems'],
    models: ['MenuItem'],
    
    refs: [
        {ref: 'menuData', selector: 'menu dataview'},
        {ref: 'viewer', selector: 'viewer'}
    ],

    init: function() {
        this.control({
            'menu dataview': {
                selectionchange: this.loadItem
            }
        });
    },
    
    onLaunch: function() {
        var dataview = this.getMenuData();
        var store = this.getMenuItemsStore();
            
        dataview.bindStore(store);
        dataview.getSelectionModel().select(store.getAt(0));
    },
    
    loadItem: function(selModel, selected) {
	    var item = selected[0];
	    
	    if (item) {
	    	this.getController(item.get('path'));
	    	this.getViewer().showTab({title: item.get('name'), xtype: item.get('view')});
	    }
    },
});
