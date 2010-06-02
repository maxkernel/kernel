package org.webcontrol.applet.ui.widget;

import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

public class MButton extends MAbstractButton implements MouseListener
{
	private static final long serialVersionUID = 1L;
	
	private boolean over, down;
	
	public MButton(String text)
	{
		super(text);
		over = down = false;
		
		addMouseListener(this);
		setColor();
	}
	
	private void setColor()
	{
		if (down)
			bgcolor = DOWN_COLOR;
		else if (over)
			bgcolor = OVER_COLOR;
		else
			bgcolor = REG_COLOR;
		
		repaint();
	}

	@Override public void mouseReleased(MouseEvent e) { down = false; setColor(); }
	@Override public void mouseEntered(MouseEvent e) { over = true; setColor(); }
	@Override public void mouseExited(MouseEvent e) { over = false; setColor(); }
	@Override public void mousePressed(MouseEvent e) { down = true; setColor(); }
	@Override public void mouseClicked(MouseEvent e) {}
}
