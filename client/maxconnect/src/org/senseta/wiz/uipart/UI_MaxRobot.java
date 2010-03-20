package org.senseta.wiz.uipart;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Image;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.swing.ImageIcon;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTable;
import javax.swing.event.AncestorEvent;
import javax.swing.event.AncestorListener;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

import org.senseta.MaxRobot;
import org.senseta.event.LayoutListener;
import org.senseta.wiz.ImageCache;

public class UI_MaxRobot implements Comparable<UI_MaxRobot>
{
	private static final Logger LOG = Logger.getLogger(UI_MaxRobot.class.getName());
	private static final long serialVersionUID = 0L;
	private static final ScheduledExecutorService UPDATER = Executors.newScheduledThreadPool(1, new ThreadFactory() {
		@Override public Thread newThread(Runnable r) { Thread t = new Thread(r); t.setDaemon(true); return t; }
	});
	private static final Color SELECTED_BG = new Color(0xBFBFFF);
	private static final Map<String, Image> ICONS;
	static {
		ICONS = new HashMap<String, Image>();
		ICONS.put("unknown", ImageCache.fromCache("unknown"));
		ICONS.put("Max 5J", ImageCache.fromCache("max5j"));
		ICONS.put("Max 5R", ImageCache.fromCache("max5r"));
	}
	
	private MaxRobot robot;
	
	private MaxPanel panel;
	private JLabel iconlabel;
	private Image icon;
	
	public UI_MaxRobot(MaxRobot robot)
	{
		this.robot = robot;
		
		icon = ICONS.get(robot.getModel());
		if (icon == null)
		{
			icon = ICONS.get("unknown");
		}
		
		panel = new MaxPanel();
		
		iconlabel = new JLabel(new ImageIcon(ImageCache.scale(getIcon(), 65, 45)));
		iconlabel.setBackground(SELECTED_BG);
		iconlabel.setOpaque(false);
	}
	
	public UI_MaxRobot()
	{
		this.robot = null;
		this.panel = null;
		icon = ICONS.get("unknown");
		
		iconlabel = new JLabel(new ImageIcon(ImageCache.scale(getIcon(), 65, 45)));
		iconlabel.setBackground(SELECTED_BG);
		iconlabel.setOpaque(false);
	}
	
	public MaxRobot getMaxRobot()
	{
		return robot;
	}
	
	private static String formatUptime(long uptime)
	{
		//round to a multiple of 5 minutes
		uptime /= (60*1000);
		long mins = uptime;
		
		if (mins < 5)
			return "< 5 minutes";
		else
			return mins+" minutes";
	}
	
	public Image getIcon()
	{
		return icon;
	}
	
	public MaxPanel getPanel()
	{
		return panel;
	}

	@Override
	public int compareTo(UI_MaxRobot o)
	{
		return robot.compareTo(o.robot);
	}
	
	@Override
	public boolean equals(Object o)
	{
		if (!(o instanceof UI_MaxRobot))
			return false;
		
		return robot.equals(((UI_MaxRobot)o).robot);
	}
	
	public class MaxPanel extends JPanel implements LayoutManager, Runnable, AncestorListener
	{
		private static final long serialVersionUID = 0L;
		
		private JLabel name, uptime, provider, ip, version;
		private ScheduledFuture<?> updater;
		
		private MaxPanel()
		{
			addAncestorListener(this);
			setBackground(SELECTED_BG);
			setOpaque(false);
			
			name = new JLabel(robot.getName());
			name.setFont(new Font("sans-serif", Font.BOLD, 18));
			
			Font subtext = new Font("sans-serif", Font.PLAIN, 11);
			
			uptime = new JLabel();
			uptime.setFont(subtext);
			
			ip = new JLabel("Unknown IP");
			ip.setFont(subtext);
			if (robot.getIP() != null)
				ip.setText("IP: "+robot.getIP());
			
			provider = new JLabel();
			provider.setFont(subtext);
			if (robot.getProvider().length() > 0)
				provider.setText("from "+robot.getProvider());
			
			version = new JLabel();
			version.setFont(new Font("sans-serif", Font.ITALIC, 9));
			if (!robot.getVersion().equals("0.0"))
				version.setText("(v"+robot.getVersion()+")");
			
			setLayout(this);
			add(name);
			add(version);
			add(ip);
			add(uptime);
			add(provider);
			
			ancestorAdded(null);
		}
		
		public void addLayoutListener(LayoutListener l)
		{
			listenerList.add(LayoutListener.class, l);
		}
		
		public void removeLayoutListener(LayoutListener l)
		{
			listenerList.remove(LayoutListener.class, l);
		}
		
		private void destroy()
		{
			updater.cancel(true);
			updater = null;
		}
		
		@Override
		public void ancestorAdded(AncestorEvent event)
		{
			if (updater == null)
			{
				updater = UPDATER.scheduleAtFixedRate(this, 0, 10, TimeUnit.SECONDS);
			}
		}
		@Override public void ancestorRemoved(AncestorEvent event) { destroy(); }
		@Override public void ancestorMoved(AncestorEvent event) {}
		
		@Override
		public void layoutContainer(Container parent)
		{
			Insets in = parent.getInsets();
			int x = in.left, y = in.top;
			int w = parent.getWidth() - in.left - in.right;
			//int h = parent.getHeight() - in.top - in.bottom;
			
			//FontMetrics fm = new FontMetrics(name.getFont());
			int nw = name.getFontMetrics(name.getFont()).stringWidth(name.getText());
			
			name.setBounds(x+5, y, w-10, 28);
			version.setBounds(x+5+nw+3, 10, 70, 15);
			provider.setBounds(x+5+nw+40, 5, 150, 22);
			uptime.setBounds(x+10, y+28, 130, 22);
			ip.setBounds(x+140, y+28, 130, 22);
		}
		
		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
		@Override public Dimension preferredLayoutSize(Container parent) { return new Dimension(300, 100); }
		@Override public Dimension minimumLayoutSize(Container parent) { return preferredLayoutSize(parent); }

		@Override
		public void run()
		{
			long uptime = robot.getUptime();
			this.uptime.setText(uptime > 0? "Uptime: "+formatUptime(uptime) : "Uptime: unknown");
			revalidate();
			fireLayoutEvent();
		}
		
		private void fireLayoutEvent()
		{
			Object[] list = listenerList.getListenerList();
			for (int i=list.length-2; i>=0; i-=2)
			{
				if (list[i] == LayoutListener.class)
				{
					try {
						((LayoutListener)list[i+1]).layoutTriggered(this);
					}
					catch (Throwable t)
					{
						LOG.log(Level.WARNING, "Event listener (ComponentListener) threw an exception", t);
					}
				}
			}
		}
	}
	
	public static void prepareColumn(TableColumn col, int i)
	{
		switch (i)
		{
			case 0:	col.setMaxWidth(70);		break;	//icon
		}
		
		col.setCellRenderer(new MaxRobotCellRenderer());
	}
	
	public static class MaxRobotTableModel extends AbstractTableModel
	{
		private static final long serialVersionUID = 0L;
		
		SortedSet<UI_MaxRobot> list;
		public MaxRobotTableModel()
		{
			this.list = new TreeSet<UI_MaxRobot>();
		}
		
		public void addRobot(UI_MaxRobot uirobot)
		{
			list.add(uirobot);
		}
		
		public void clearList()
		{
			for (UI_MaxRobot m : list)
			{
				m.panel.destroy();
			}
			
			list.clear();
		}
		
		public SortedSet<UI_MaxRobot> getList()
		{
			return Collections.unmodifiableSortedSet(list);
		}
		
		@Override public Class<?> getColumnClass(int columnIndex) { return UI_MaxRobot.class; }
		@Override public int getColumnCount() { return 2; }
		@Override public String getColumnName(int columnIndex) { return new String[]{"Model","Name"}[columnIndex]; }
		@Override public int getRowCount() { return list.size(); }
		@Override public Object getValueAt(int rowIndex, int columnIndex) { return getItemAtIndex(rowIndex); }
		@Override public boolean isCellEditable(int row, int col) { return false; }
		
		private UI_MaxRobot getItemAtIndex(int i) throws IndexOutOfBoundsException
		{
			if (i < 0 || i > list.size())
				throw new IndexOutOfBoundsException("Invalid index "+i);
			
			UI_MaxRobot robot = null;
			Iterator<UI_MaxRobot> itr = list.iterator();
			
			do
			{
				robot = itr.next();
			}
			while (--i >= 0);
			
			return robot;
		}
	}
	
	public static class MaxRobotCellRenderer implements TableCellRenderer
	{
		@Override
		public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column)
		{
			UI_MaxRobot ui = (UI_MaxRobot)value;
			
			JComponent widget = null;
			switch (column)
			{
				case 0:	widget = ui.iconlabel;	break;
				case 1:	widget = ui.panel;		break;
			}
			
			if (widget == null)
				throw new IllegalArgumentException("Invalid column: "+column);
			
			widget.setOpaque(isSelected);
			return widget;
		}
	}
}
