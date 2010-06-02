package org.webcontrol.applet.ui.widget;

import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

import org.webcontrol.applet.ui.widget.event.ButtonListener;

public class MToggleButton extends MAbstractButton implements MouseListener
{
	private static final long serialVersionUID = 1L;
	
	private boolean isselected;
	private boolean over, down;
	
	public MToggleButton(String text)
	{
		super(text);
		isselected = over = down = false;
		
		addMouseListener(this);
		addButtonListener(new ButtonListener() {
			@Override
			public void buttonPressed(MAbstractButton b) {
				isselected = !isselected;
				setColor();
			}
		});
		setColor();
	}
	
	public boolean isSelected()
	{
		return isselected;
	}
	
	private void setColor()
	{
		if (down)
			bgcolor = DOWN_COLOR;
		else if (over || isselected)
			bgcolor = OVER_COLOR;
		else
			bgcolor = REG_COLOR;
		
		repaint();
	}

	@Override public void mouseReleased(MouseEvent e) { down = false; over = false; setColor(); }
	@Override public void mouseEntered(MouseEvent e) { over = true; setColor(); }
	@Override public void mouseExited(MouseEvent e) { over = false; setColor(); }
	@Override public void mousePressed(MouseEvent e) { down = true; setColor(); }
	@Override public void mouseClicked(MouseEvent e) {}
}
