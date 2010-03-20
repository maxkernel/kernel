package org.senseta;

import java.awt.Component;
import java.awt.Container;
import java.awt.Cursor;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Image;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.SwingUtilities;
import javax.swing.border.BevelBorder;

import org.senseta.wiz.BundleWizard;
import org.senseta.wiz.DeployWizard;
import org.senseta.wiz.ImageCache;
import org.senseta.wiz.RunWizard;
import org.senseta.wiz.ServiceWizard;

public class MaxConnect extends JPanel implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	private MaxMenu[] items;
	private String[] args;
	
	public MaxConnect(String[] a)
	{
		args = a;
		items = new MaxMenu[5];
		items[0] = new MaxMenu("Run Wizard", ImageCache.fromCache("run"), new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				RunWizard.main(args);
				dispose();
			}
		});
		items[1] = new MaxMenu("Service Wizard", ImageCache.fromCache("service"), new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				ServiceWizard.main(args);
				dispose();
			}
		});
		items[2] = new MaxMenu("Deploy Wizard", ImageCache.fromCache("deploy"), new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				DeployWizard.main(args);
				dispose();
			}
		});
		items[3] = new MaxMenu("Max Admin", ImageCache.fromCache("admin"), new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				
			}
		});
		items[4] = new MaxMenu("Bundle Builder", ImageCache.fromCache("bundle"), new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				BundleWizard.main(args);
				dispose();
			}
		});
		
		
		setLayout(this);
		for (int i=0; i<items.length; i++)
		{
			add(items[i]);
		}
	}
	
	private void dispose()
	{
		((JFrame)getTopLevelAncestor()).dispose();
	}
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		
		int height = h / items.length;
		for (int i=0; i<items.length; i++)
		{
			items[i].setBounds(x, y+(i*height), w, height);
		}
	}
	
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	
	private static class MaxMenu extends JPanel implements LayoutManager, MouseListener
	{
		private static final long serialVersionUID = 0L;
		private static final Font TEXT = new Font("sans-serif", Font.PLAIN, 18);
		
		private JLabel image, text;
		private ActionListener listener;
		
		public MaxMenu(String name, Image icon, ActionListener a)
		{
			setCursor(new Cursor(Cursor.HAND_CURSOR));
			addMouseListener(this);
			listener = a;
			
			image = new JLabel(new ImageIcon(ImageCache.scale(icon, 50, 50)));
			image.setOpaque(false);
			
			text = new JLabel(name);
			text.setOpaque(false);
			text.setFont(TEXT);
			
			setLayout(this);
			add(image);
			add(text);
		}
		
		@Override
		public void layoutContainer(Container parent) {
			Insets in = parent.getInsets();
			int x = in.left, y = in.top;
			int w = parent.getWidth() - in.left - in.right;
			int h = parent.getHeight() - in.top - in.bottom;
			
			image.setBounds(x, y, 100, h);
			text.setBounds(x+120, y, w-150, h);
		}
		
		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
		@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
		@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }

		@Override
		public void mouseClicked(MouseEvent e) {
			listener.actionPerformed(new ActionEvent(e.getSource(), e.getID(), ""));
		}

		@Override public void mouseEntered(MouseEvent e) { setBorder(BorderFactory.createBevelBorder(BevelBorder.RAISED)); }
		@Override public void mouseExited(MouseEvent e) { setBorder(null); }
		@Override public void mousePressed(MouseEvent e) { setBorder(BorderFactory.createBevelBorder(BevelBorder.LOWERED)); }
		@Override public void mouseReleased(MouseEvent e) {}
	}
	
	public static void main(final String[] args)
	{
		for (String arg : args)
		{
			if (arg.equals("run"))
			{
				RunWizard.main(args);
				return;
			}
			else if (arg.equals("service"))
			{
				ServiceWizard.main(args);
				return;
			}
			else if (arg.equals("deploy"))
			{
				DeployWizard.main(args);
				return;
			}
			else if (arg.equals("admin"))
			{
				//TODO
				return;
			}
			else if (arg.equals("bundle"))
			{
				BundleWizard.main(args);
				return;
			}
		}
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run() {
				JFrame f = new JFrame("MaxConnect");
				f.setContentPane(new MaxConnect(args));
				f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				
				f.pack();
				f.setSize(400, 300);
				f.setMinimumSize(new Dimension(400, 300));
				f.setLocationRelativeTo(null);
				f.setVisible(true);
			}
		});
	}
}
