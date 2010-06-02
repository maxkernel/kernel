package org.webcontrol.applet.ui;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;

import javax.swing.JLayeredPane;

import org.webcontrol.applet.ui.util.DragArea;
import org.webcontrol.applet.ui.widget.MWindow;
import org.webcontrol.applet.ui.widget.event.WindowListener;

public class MainPanel extends JLayeredPane implements LayoutManager, WindowListener
{	
	private static final long serialVersionUID = 1L;
	
	DragArea da1, da2;
	public MainPanel()
	{
		MWindow w1 = new MWindow("Window 1");
		w1.addWindowListener(this);
		
		MWindow w2 = new MWindow("Window 2");
		w2.addWindowListener(this);
		
		da1 = new DragArea(w1, 10000, 10, 150, 150);
		da2 = new DragArea(w2, 10000, 180, 150, 150);
		setLayout(this);
		add(da1, new Integer(2), 0);
		add(da2, new Integer(2), 1);
	}
	
	@Override
	public void windowSelected(MWindow window) {
		moveToFront(window.getParent());
	}
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		da1.setBounds(x, y, w, h);
		da2.setBounds(x, y, w, h);
	}
	
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
}
