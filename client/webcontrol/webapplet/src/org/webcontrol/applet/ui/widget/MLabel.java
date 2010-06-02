package org.webcontrol.applet.ui.widget;

import java.awt.Color;
import java.awt.Font;

import javax.swing.JLabel;
import javax.swing.SwingConstants;

public class MLabel extends JLabel implements SwingConstants
{
	private static final long serialVersionUID = 1L;
	private static final Font FONT = new Font(Font.SANS_SERIF, Font.PLAIN, 11);
	
	public MLabel(String text, int alignment)
	{
		super(text, alignment);
		setBackground(Color.BLACK);
		setForeground(Color.GRAY);
		setOpaque(false);
		setFont(FONT);
	}
	
	public MLabel(String text)
	{
		this(text, CENTER);
	}
}
