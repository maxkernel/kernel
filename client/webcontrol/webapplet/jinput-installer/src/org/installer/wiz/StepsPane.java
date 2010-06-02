package org.installer.wiz;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.GradientPaint;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.geom.Point2D;

import javax.swing.BorderFactory;
import javax.swing.JLabel;
import javax.swing.JPanel;

public class StepsPane extends JPanel implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	private JLabel background;
	private JLabel[] labels;
	
	private Font plain, bold;
	private Color bgcolor;
	
	public StepsPane(final String[] steps)
	{
		bgcolor = Color.GRAY;
		
		setLayout(this);
		setBorder(BorderFactory.createLineBorder(bgcolor.darker(), 1));
		
		plain = new Font("sans-serif", Font.PLAIN, 12);
		bold = new Font("sans-serif", Font.BOLD, 12);
		
		labels = new JLabel[steps.length];
		for (int i=0; i<steps.length; i++)
		{
			labels[i] = new JLabel("" + (i+1) + ". "+steps[i]);
			labels[i].setFont(plain);
			labels[i].setOpaque(false);
			add(labels[i]);
		}
		
		background = new JLabel() {
			private static final long serialVersionUID = 0L;
			
			@Override
			public void paint(Graphics g)
			{
				super.paint(g);
				Graphics2D g2 = (Graphics2D)g;
				
				float[] b1 = Color.WHITE.getColorComponents(new float[3]);
				float[] b2 = bgcolor.getColorComponents(new float[3]);
				Color blend = new Color(
					(b1[0] + b2[0]) * 0.5F,
					(b1[1] + b2[1]) * 0.5F,
					(b1[2] + b2[2]) * 0.5F
				);
				
				GradientPaint p = new GradientPaint(new Point2D.Double(0.0, 0.0), Color.WHITE, new Point2D.Double(0.0, getHeight()), blend);
				g2.setPaint(p);
				g2.fillRect(0, 0, getWidth(), getHeight());
			}
		};
		background.setOpaque(true);
		add(background);
	}
	
	public void setOnStep(int stepnum)
	{
		for (int i=0; i<labels.length; i++)
		{
			labels[i].setFont((i == stepnum)? bold : plain);
		}
		
		invalidate();
	}
	
	public void setBarColor(Color bg)
	{
		bgcolor = bg;
		
		setBorder(BorderFactory.createLineBorder(bgcolor.darker(), 1));
		background.repaint();
	}

	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		background.setBounds(x, y, w, h);
		
		int offset = 20;
		for (int i=0; i<labels.length; i++)
		{
			labels[i].setBounds(x + 20, y + offset, w-30, 15);
			offset += 20;
		}
	}

	@Override
	public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }

	@Override
	public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }

	@Override public void removeLayoutComponent(Component comp) {}
	@Override public void addLayoutComponent(String name, Component comp) {}
}
