package org.webcontrol.applet.ui;

import java.awt.Color;
import java.awt.Graphics;
import java.awt.Point;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseMotionListener;
import java.awt.geom.Point2D;

import javax.swing.BorderFactory;
import javax.swing.JPanel;

import org.webcontrol.applet.joystick.Joystick;

public class DriveStick extends JPanel implements MouseListener, MouseMotionListener
{
	private static final long serialVersionUID = 0L;
	
	private Point2D.Double pos;
	private boolean mousedown;
	
	public DriveStick()
	{
		pos = new Point2D.Double(0.0, 0.0);
		mousedown = false;
		
		setOpaque(true);
		setBackground(Color.WHITE);
		setBorder(BorderFactory.createLineBorder(Color.BLACK));
		
		addMouseListener(this);
		addMouseMotionListener(this);
	}
	
	private static double clamp(double x, double min, double max)
	{
		return Math.max(Math.min(x, max), min);
	}
	
	public void updateWithJoystick(Joystick js)
	{
		if (mousedown)
		{
			//don't interfere with the mouse
			return;
		}
		
		setPosition(-js.getR(), -js.getY());
	}
	
	public void resetPosition()
	{
		setPosition(pos.x, 0.0);
	}
	
	public void setPosition(double x, double y)
	{
		pos.x = clamp(x, -1, 1);
		pos.y = clamp(y, -1, 1);
		repaint();
	}
	
	private void setPosition(Point p)
	{
		setPosition(-(p.x - getWidth()/2.0)/(getWidth()/2.0), -(p.y - getHeight()/2.0)/(getHeight()/2.0));
	}
	
	public double getXPosition() { return pos.x; }
	public double getYPosition() { return pos.y; }
	
	@Override
	public void paintComponent(Graphics g)
	{
		super.paintComponent(g);
		
		g.setColor(Color.BLUE);
		g.drawLine(getWidth()/2, 0, getWidth()/2, getHeight());
		g.drawLine(0, getHeight()/2, getWidth(), getHeight()/2);
		
		int px = (int)(getWidth()/2.0 * -pos.x + getWidth()/2.0);
		int py = (int)(getHeight()/2.0 * -pos.y + getHeight()/2.0);
		g.setColor(Color.RED.darker());
		g.fillOval(px-4, py-4, 8, 8);
	}
	
	
	@Override public void mouseReleased(MouseEvent e) { mousedown = false; resetPosition(); }
	@Override public void mousePressed(MouseEvent e) { mousedown = true; setPosition(e.getPoint()); }
	@Override public void mouseDragged(MouseEvent e) { setPosition(e.getPoint()); }

	@Override public void mouseClicked(MouseEvent e) {}
	@Override public void mouseEntered(MouseEvent e) {}
	@Override public void mouseExited(MouseEvent e) {}
	@Override public void mouseMoved(MouseEvent e) {}
}
