package org.webcontrol.applet.ui.util;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;

import javax.swing.JComponent;
import javax.swing.JPanel;

public class CenteredLayout extends JPanel implements LayoutManager
{
	private static final long serialVersionUID = 1L;
	
	private int width, height;
	private JComponent component;
	
	public CenteredLayout(JComponent c, int width, int height)
	{
		this.width = width;
		this.height = height;
		this.component = c;
		
		setOpaque(false);
		setLayout(this);
		add(component);
	}
	
	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		int xpos = (w-width)/2;
		int ypos = (h-height)/2;
		
		component.setBounds(x+xpos, y+ypos, width, height);
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
