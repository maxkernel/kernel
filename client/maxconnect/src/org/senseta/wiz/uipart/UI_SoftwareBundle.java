package org.senseta.wiz.uipart;

import java.awt.Color;
import java.awt.Component;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.SortedSet;
import java.util.TreeSet;

import javax.swing.ImageIcon;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

import org.senseta.SoftwareBundle;
import org.senseta.SoftwareCache;
import org.senseta.wiz.ImageCache;

public class UI_SoftwareBundle implements Comparable<UI_SoftwareBundle>
{
	private static final Color SELECTED_BG = new Color(0xBFBFFF);
	
	private SoftwareBundle bundle;
	
	private ImageIcon icon;
	
	public UI_SoftwareBundle(SoftwareBundle bundle)
	{
		this.bundle = bundle;
		
		icon = new ImageIcon(ImageCache.scale(SoftwareCache.getIcon(bundle), 30, 26));
	}
	
	public SoftwareBundle getSoftwareBundle()
	{
		return bundle;
	}

	@Override
	public int compareTo(UI_SoftwareBundle o)
	{
		return bundle.compareTo(o.bundle);
	}
	
	@Override
	public boolean equals(Object o)
	{
		if (!(o instanceof UI_SoftwareBundle))
			return false;
		
		return bundle.equals(((UI_SoftwareBundle)o).bundle);
	}
	
	public static void prepareColumn(TableColumn col, int i)
	{
		switch (i)
		{
			case 0:	col.setMaxWidth(35);;			break;	//icon
			case 1: col.setPreferredWidth(220);		break;	//name
		}
		
		col.setCellRenderer(new SoftwareBundleCellRenderer());
	}
	
	public static class SoftwareBundleTableModel extends AbstractTableModel
	{
		private static final long serialVersionUID = 0L;
		
		SortedSet<UI_SoftwareBundle> list;
		public SoftwareBundleTableModel(List<SoftwareBundle> l)
		{
			list = new TreeSet<UI_SoftwareBundle>();
			for (SoftwareBundle b : l)
			{
				list.add(b.getUIPart());
			}
		}
		
		public void addBundle(SoftwareBundle bundle)
		{
			list.add(bundle.getUIPart());
		}
		
		public void removeBundle(SoftwareBundle bundle)
		{
			list.remove(bundle.getUIPart());
		}
		
		public SortedSet<UI_SoftwareBundle> getList()
		{
			return Collections.unmodifiableSortedSet(list);
		}
		
		@Override public Class<?> getColumnClass(int columnIndex) { return UI_MaxRobot.class; }
		@Override public int getColumnCount() { return 3; }
		@Override public String getColumnName(int columnIndex) { return new String[]{"", "Name","Version"}[columnIndex]; }
		@Override public int getRowCount() { return list.size(); }
		@Override public Object getValueAt(int rowIndex, int columnIndex) { return getItemAtIndex(rowIndex); }
		@Override public boolean isCellEditable(int row, int col) { return false; }
		
		private UI_SoftwareBundle getItemAtIndex(int i) throws IndexOutOfBoundsException
		{
			if (i < 0 || i > list.size())
				throw new IndexOutOfBoundsException("Invalid index "+i);
			
			UI_SoftwareBundle bundle = null;
			Iterator<UI_SoftwareBundle> itr = list.iterator();
			
			do
			{
				bundle = itr.next();
			}
			while (--i >= 0);
			
			return bundle;
		}
	}
	
	private static class SoftwareBundleCellRenderer implements TableCellRenderer
	{
		private JLabel l;
		
		private SoftwareBundleCellRenderer()
		{
			l = new JLabel();
			l.setOpaque(false);
			l.setBackground(SELECTED_BG);
		}
		
		@Override
		public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column)
		{
			UI_SoftwareBundle s = (UI_SoftwareBundle)value;
			
			l.setOpaque(isSelected);
			switch (column)
			{
				case 0: l.setIcon(s.icon);									break;
				case 1:	l.setText(s.bundle.getName());						break;
				case 2: l.setText(Double.toString(s.bundle.getVersion()));	break;
			}
			
			return l;
		}
	}
}
