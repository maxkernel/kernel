package org.webcontrol.applet.ui.util;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.LayoutManager;
import java.awt.Point;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseMotionListener;

import javax.swing.JComponent;
import javax.swing.JPanel;

public class DragArea extends JPanel implements LayoutManager, MouseListener, MouseMotionListener
{
	
	private JComponent component;
	private int width, height;
	private int xoffset, yoffset;
	private Point lastpoint;
	
	public DragArea(JComponent c, int x, int y, int w, int h)
	{
		lastpoint = null;
		component = c;
		xoffset = x;
		yoffset = y;
		width = w;
		height = h;
		
		setOpaque(false);
		c.addMouseListener(this);
		c.addMouseMotionListener(this);
		
		setLayout(this);
		add(component);
	}

	

	@Override
	public void layoutContainer(Container parent) {
		component.setBounds(CLAMP(xoffset, 0, getWidth()-width), CLAMP(yoffset, 0, getHeight()-height), width, height);
	}

	@Override
	public void mousePressed(MouseEvent e) {
		lastpoint = e.getPoint();
	}

	@Override
	public void mouseReleased(MouseEvent e) {
		lastpoint = null;
	}

	@Override
	public void mouseDragged(MouseEvent e) {
		if (lastpoint != null)
		{
			xoffset = CLAMP(xoffset + e.getPoint().x - lastpoint.x, 0, getWidth()-width);
			yoffset = CLAMP(yoffset + e.getPoint().y - lastpoint.y, 0, getHeight()-height);
			doLayout();
		}
	}
	
	private int CLAMP(int x, int low, int high)
	{
		return Math.min(Math.max(x, low), high);
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return new Dimension(parent.getWidth(), parent.getHeight()); }

	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public void mouseClicked(MouseEvent e) {}
	@Override public void mouseEntered(MouseEvent e) {}
	@Override public void mouseExited(MouseEvent e) {}
	@Override public void mouseMoved(MouseEvent e) {}
}
