Ext.define('Max.controller.Menu', {
    extend: 'Ext.app.Controller',

    stores: ['MenuItems'],
    models: ['MenuItem'],
    
    refs: [
        {ref: 'menuData', selector: 'menu dataview'},
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
            var c = this.getController(item.get('path'));
            Ext.onReady(function() {
                c.init(this);
            }, this);          
        }
    }
});
