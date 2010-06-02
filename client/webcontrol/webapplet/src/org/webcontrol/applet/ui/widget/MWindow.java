package org.webcontrol.applet.ui.widget;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

import javax.swing.Icon;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JPanel;

import org.webcontrol.applet.ui.widget.event.WindowListener;

public class MWindow extends JPanel
{
	private static final long serialVersionUID = 1L;
	private static final Font FONT = new Font(Font.SANS_SERIF, Font.BOLD, 13);
	protected static final Color BGCOLOR = new Color(0, 0, 0, 100);
	protected static final Color FGCOLOR = Color.DARK_GRAY;
	
	private MLabel title;
	private JComponent content;
	
	public MWindow(String thetitle, Icon icon, JComponent c)
	{
		addMouseListener(new MouseAdapter() {
			@Override
			public void mousePressed(MouseEvent e) {
				fireWindowSelected();
			}
		});
		
		title = new MLabel(thetitle, JLabel.LEFT);
		title.setFont(FONT);
		title.setForeground(FGCOLOR.brighter());
		title.setIcon(icon);
		
		if (c == null)
		{
			content = new JPanel();
			content.setOpaque(false);
			content.setLayout(new BorderLayout());
		}
		else
		{
			content = c;
		}
		
		setOpaque(false);
		setLayout(new LayoutManager() {
			@Override
			public void layoutContainer(Container parent) {
				Insets in = parent.getInsets();
				int x = in.left, y = in.top;
				int w = parent.getWidth() - in.left - in.right;
				int h = parent.getHeight() - in.top - in.bottom;
				
				title.setBounds(x+10, y+3, w-15, 22);
				content.setBounds(x+2, y+25, w-4, h-27);
			}
			
			@Override public void addLayoutComponent(String name, Component comp) {}
			@Override public void removeLayoutComponent(Component comp) {}
			@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
			@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
		});
		add(title);
		add(content);
	}
	
	public MWindow(String thetitle)
	{
		this(thetitle, null, null);
	}
	
	public MWindow(String thetitle, JComponent c)
	{
		this(thetitle, null, c);
	}
	
	public void setContentPane(JComponent c)
	{
		remove(content);
		add(content);
		validate();
	}
	
	public JComponent getContentPane()
	{
		return content;
	}
	
	@Override
	public void paintComponent(Graphics g)
	{
		super.paintComponent(g);
		g.setColor(BGCOLOR);
		g.fillRoundRect(0, 0, getWidth()-1, getHeight()-1, 5, 5);
		g.setColor(FGCOLOR);
		g.drawRoundRect(0, 0, getWidth()-1, getHeight()-1, 5, 5);
	}
	
	private void fireWindowSelected()
	{
		Object[] listeners = listenerList.getListenerList();
		for (int i=0; i<=listeners.length-2; i+=2) {
			if (listeners[i]==WindowListener.class) {
				((WindowListener)listeners[i+1]).windowSelected(this);
			}
		}
	}
	
	public void addWindowListener(WindowListener l) { listenerList.add(WindowListener.class, l); }
	public void removeWindowListener(WindowListener l) { listenerList.remove(WindowListener.class, l); }
}
