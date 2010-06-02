package org.installer.wiz;

import java.awt.CardLayout;
import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.SwingUtilities;

import org.installer.ImageCache;
import org.installer.wiz.WizardPagePart;

public class Wizard extends JFrame
{
	private static final long serialVersionUID = 0L;
	
	public enum ViewPolicy { WIZARD, SIMPLE };
	
	private WizardPanel panel;
	
	public Wizard(String title, WizardPagePart[] pages, ViewPolicy view)
	{
		super(title);
		
		panel = new WizardPanel(pages, view);
		
		setMinimumSize(new Dimension(400, 350));
		setContentPane(panel);
	}
	
	public void setBarColor(Color color)
	{
		if (panel.steps != null)
		{
			panel.steps.setBarColor(color);
		}
	}
	
	private static class WizardPanel extends JPanel implements LayoutManager, ActionListener
	{
		private static final long serialVersionUID = 0L;
		
		private HeaderPane header;
		private StepsPane steps;
		private JPanel cards;
		private JButton next, back, skip, cancel;
		
		private Map<String, Object> properties;
		private WizardPagePart[] pages;
		private int onpage;
		
		private WizardPanel(WizardPagePart[] pages, ViewPolicy view)
		{
			this.pages = pages;
			properties = new HashMap<String, Object>();
			
			setOpaque(true);
			setBackground(new Color(0xEBEBEB));
			
			header = new HeaderPane();
			
			skip = new JButton("Skip", new ImageIcon(ImageCache.fromCache("skip")));
			skip.setVisible(false);
			skip.addActionListener(new ActionListener() {
				@Override
				public void actionPerformed(ActionEvent e) {
					WizardPanel.this.pages[onpage].skipped(properties);
					next.requestFocus();
					setPage(onpage + 1);
				}
			});
			
			next = new JButton("Next >");
			next.addActionListener(this);
			SwingUtilities.invokeLater(new Runnable() {
				@Override
				public void run()
				{
					next.requestFocus();
				}
			});
			
			back = new JButton("< Back");
			back.addActionListener(new ActionListener() {
				@Override
				public void actionPerformed(ActionEvent e)
				{
					setPage(onpage -1);
				}
			});
			
			cancel = new JButton("Cancel");
			cancel.addActionListener(new ActionListener() {
				@Override
				public void actionPerformed(ActionEvent e)
				{
					System.exit(0);
				}
			});
			
			List<String> names = new ArrayList<String>();
			cards = new JPanel();
			cards.setLayout(new CardLayout());
			for (WizardPagePart page : pages)
			{
				cards.add(page.getName(), page);
				page.addActionListener(this);
				names.add(page.getName());
			}
			
			if (view == ViewPolicy.WIZARD)
			{
				steps = new StepsPane(names.toArray(new String[names.size()]));
			}
			
			setLayout(this);
			add(header);
			add(cards);
			add(back);
			add(next);
			add(skip);
			
			if (view == ViewPolicy.WIZARD)
			{
				add(steps);
				add(cancel);
			}
			
			setPage(0);
		}
		
		public void setPage(final int pagenum)
		{
			if (pagenum == pages.length)
			{
				System.exit(0);
			}
			
			if (steps != null)
			{
				steps.setOnStep(pagenum);
			}
			
			back.setEnabled(pagenum != 0);
			if (pagenum == pages.length-1)
			{
				back.setEnabled(false);
				cancel.setEnabled(false);
			}
			
			if (pagenum == pages.length-1)
			{
				next.setText("Finish");
			}
			
			header.setText(pages[pagenum].getName(), pages[pagenum].getDescription());
			
			((CardLayout)cards.getLayout()).show(cards, pages[pagenum].getName());
			
			int lastpage = onpage;
			onpage = pagenum;
			
			skip.setVisible(pages[onpage].isSkippable());
			pages[lastpage].setVisible(properties, false);
			pages[onpage].setVisible(properties, true);
			
			revalidate();
		}
		
		@Override
		public void actionPerformed(ActionEvent e)
		{
			if (pages[onpage].validate(properties))
			{
				next.requestFocus();
				setPage(onpage + 1);
			}
		}
		
		@Override
		public void paint(Graphics g)
		{
			super.paint(g);
			
			g.setColor(Color.DARK_GRAY);
			g.drawLine(0, getHeight()-50, getWidth(), getHeight()-50);
			
			g.setColor(Color.LIGHT_GRAY);
			g.drawLine(0, getHeight()-49, getWidth(), getHeight()-49);
		}
		
		@Override
		public void layoutContainer(Container parent)
		{
			Insets in = parent.getInsets();
			int x = in.left, y = in.top;
			int w = parent.getWidth() - in.left - in.right;
			int h = parent.getHeight() - in.top - in.bottom;
			
			int offset = steps == null? 0 : (int)(w*0.3d);
			int width = steps == null? w : (int)(w*0.7d);
			
			if (steps != null)
			{
				steps.setBounds(x, y, (int)(w*0.3d), h-50);
				back.setBounds(w-280, h-35, 80, 25);
				next.setBounds(w-200, h-35, 80, 25);
				cancel.setBounds(w-100, h-35, 80, 25);
			}
			else
			{
				back.setBounds(w-180, h-35, 80, 25);
				next.setBounds(w-100, h-35, 80, 25);
			}
			
			skip.setBounds(x+20, h-35, 80, 25);
			header.setBounds(offset, y, width, 80);
			cards.setBounds(offset, y+80, width, h-130);
			
		}

		@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(300, 200); }
		@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
	}
}
