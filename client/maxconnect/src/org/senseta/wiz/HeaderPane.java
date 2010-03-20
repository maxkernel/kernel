package org.senseta.wiz;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Insets;
import java.awt.LayoutManager;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextArea;

public class HeaderPane extends JPanel implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	private JLabel title;
	private JTextArea description;
	
	public HeaderPane()
	{
		setLayout(this);
		setOpaque(true);
		setBackground(Color.WHITE);
		
		title = new JLabel();
		title.setFont(new Font("sans-serif", Font.BOLD, 14));
		
		description = new JTextArea();
		description.setFont(new Font("sans-serif", Font.PLAIN, 12));
		description.setEditable(false);
		description.setBorder(null);
		description.setWrapStyleWord(true);
		description.setLineWrap(true);
		
		add(title);
		add(description);
	}
	
	public void setText(String title, String desc)
	{
		this.title.setText(title);
		description.setText(desc);
	}
	
	@Override
	public void paint(Graphics g)
	{
		super.paint(g);
		
		g.setColor(Color.DARK_GRAY);
		g.drawLine(0, getHeight()-2, getWidth(), getHeight()-2);
		
		g.setColor(Color.LIGHT_GRAY);
		g.drawLine(0, getHeight()-1, getWidth(), getHeight()-1);
	}
	
	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		title.setBounds(x+10, y+5, w-100, 20);
		description.setBounds(x+30, y+30, w-100, h-35);
		
	}
	
	@Override
	public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }

	@Override
	public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }

	@Override public void removeLayoutComponent(Component comp) {}
	@Override public void addLayoutComponent(String name, Component comp) {}
}
