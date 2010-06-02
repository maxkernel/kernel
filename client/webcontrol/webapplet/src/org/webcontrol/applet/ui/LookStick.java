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

public class LookStick extends JPanel implements MouseListener, MouseMotionListener
{
	private static final long serialVersionUID = 0L;
	private static final double VERT_SCALE = 0.05;
	private static final double HORIZ_SCALE = 0.03;
	
	private Point2D.Double pos;
	
	public LookStick()
	{
		pos = new Point2D.Double(0.0, 0.0);
		
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
		if (js.getTrigger())
		{
			setPosition(0, 0);
			return;
		}
		
		double pov = js.getPOV() * Math.PI * 2.0;
		if (pov == 0)
		{
			return;
		}
		
		double x = -Math.cos(pov) * HORIZ_SCALE;
		double y = Math.sin(pov) * VERT_SCALE;
		
		setPosition(x+pos.x, y+pos.y);
		
		//System.out.println("("+x+","+y+")");
	}
	
	public void setPosition(double x, double y)
	{
		pos.y = clamp(y, -1, 1);
		while (x < -1) x += 2;
		while (x > 1) x -= 2;
		pos.x = x;
		repaint();
		
		//System.out.println("("+pos.x+","+pos.y+")");
	}
	
	private void setPosition(Point p)
	{
		double px, py;
		py = -(p.y - getHeight()/2.0)/(getHeight()/2.0);
		if (p.x < getWidth()/4)
			px = (0.5/(getWidth()*0.25))*p.x+0.5;
		else
			px = (1.5/(getWidth()*0.75)*p.x-1.5);
		
		setPosition(px, py);
	}
	
	public double getXPosition() { return pos.x; }
	public double getYPosition() { return pos.y; }
	
	@Override
	public void paintComponent(Graphics g)
	{
		super.paintComponent(g);
		
		g.setColor(Color.BLUE);
		g.drawLine((int)(getWidth()*0.25), 0, (int)(getWidth()*0.25), getHeight());
		g.drawLine((int)(getWidth()*0.75), 0, (int)(getWidth()*0.75), getHeight());
		g.drawLine(0, getHeight()/2, getWidth(), getHeight()/2);
		
		int px;
		if (pos.x > 0.5)
			px = (int)((getWidth()*0.25)/0.5*pos.x-(getWidth()*0.25));
		else
			px = (int)((getWidth()*0.75)/1.5*pos.x+(getWidth()*0.75));
		
		int py = (int)(getHeight()/2.0 * -pos.y + getHeight()/2.0);
		g.setColor(Color.RED.darker());
		g.fillOval(px-4, py-4, 8, 8);
	}
	
	@Override public void mousePressed(MouseEvent e) { setPosition(e.getPoint()); }
	@Override public void mouseDragged(MouseEvent e) { setPosition(e.getPoint()); }

	@Override public void mouseClicked(MouseEvent e) {}
	@Override public void mouseReleased(MouseEvent e) {}
	@Override public void mouseEntered(MouseEvent e) {}
	@Override public void mouseExited(MouseEvent e) {}
	@Override public void mouseMoved(MouseEvent e) {}
}
