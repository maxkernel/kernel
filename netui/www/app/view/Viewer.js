Ext.define('Max.view.Viewer', {
    extend: 'Ext.tab.Panel',
    alias: 'widget.viewer',
    
    activeItem: 0,
    margins: '5 5 5 5',
    
    tabs: [],
    
    initComponent: function() {
        this.callParent(arguments);
    },
    
    closeTab: function(fireEvent, config) {
        this.tabs[config.title].destroy();
        this.tabs[config.title] = undefined;
    },
    
    showTab: function(tabConfig) {
        if (this.tabs[tabConfig.title] !== undefined) {
            this.setActiveTab(this.tabs[tabConfig.title]);
            return;
        }
        
        this.tabs[tabConfig.title] = this.add(tabConfig);
        this.tabs[tabConfig.title].on('beforeclose', this.closeTab, this, { title: tabConfig.title });
    },
    
    
});
