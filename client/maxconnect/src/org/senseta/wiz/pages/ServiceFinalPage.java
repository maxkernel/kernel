package org.senseta.wiz.pages;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.util.Map;

public class ServiceFinalPage extends WizardPagePart implements LayoutManager
{
	
	
	@Override public String getName() { return "Services Launcher"; }
	@Override public String getDescription() { return "This will connect and launch the services browser"; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public void setVisible(Map<String, Object> properties, boolean isvisible) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean validate(Map<String, Object> properties) {
		return true;
	}
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
	}

	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
}
