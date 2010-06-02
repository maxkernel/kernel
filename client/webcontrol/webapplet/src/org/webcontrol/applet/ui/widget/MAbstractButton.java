package org.webcontrol.applet.ui.widget;

import java.awt.Color;
import java.awt.Cursor;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.geom.Rectangle2D;

import javax.swing.JPanel;

import org.webcontrol.applet.ui.widget.event.ButtonListener;

public abstract class MAbstractButton extends JPanel
{
	private static final long serialVersionUID = 1L;
	private static final Font FONT = new Font(Font.SANS_SERIF, Font.PLAIN, 11);
	private static final Cursor CURSOR = Cursor.getPredefinedCursor(Cursor.HAND_CURSOR);
	protected static final Color DOWN_COLOR = Color.LIGHT_GRAY;
	protected static final Color OVER_COLOR = Color.GRAY;
	protected static final Color REG_COLOR = Color.DARK_GRAY;
	
	protected String text;
	protected Color bgcolor;
	
	public MAbstractButton(String text)
	{
		this.text = text;
		this.bgcolor = Color.BLACK;
		
		setCursor(CURSOR);
		setOpaque(false);
		addMouseListener(new MouseAdapter() {
			@Override
			public void mouseReleased(MouseEvent e)
			{
				if (contains(e.getPoint()))
				{
					fireButtonPressed();
				}
			}
		});
	}
	
	@Override
	public void paintComponent(Graphics g)
	{
		super.paintComponent(g);
		
		g.setColor(bgcolor);
		g.fillRoundRect(3, 3, getWidth()-6, getHeight()-6, 5, 5);
		g.setColor(bgcolor.brighter());
		g.drawRoundRect(3, 3, getWidth()-6, getHeight()-6, 5, 5);
		
		FontMetrics fm = g.getFontMetrics(FONT);
		Rectangle2D rect = fm.getStringBounds(text, g);
		int x = (int)(getWidth()-rect.getWidth())/2;
		int y = (int)((getHeight()-rect.getHeight())/2.0 + rect.getHeight() - 2);
		
		g.setColor(Color.BLACK);
		g.setFont(FONT);
		g.drawString(text, x, y);
	}
	
	private void fireButtonPressed()
	{
		Object[] listeners = listenerList.getListenerList();
		for (int i=0; i<=listeners.length-2; i+=2) {
			if (listeners[i]==ButtonListener.class) {
				((ButtonListener)listeners[i+1]).buttonPressed(this);
			}
		}
	}
	
	public void addButtonListener(ButtonListener l) { listenerList.add(ButtonListener.class, l); }
	public void removeButtonListener(ButtonListener l) { listenerList.remove(ButtonListener.class, l); }
}
