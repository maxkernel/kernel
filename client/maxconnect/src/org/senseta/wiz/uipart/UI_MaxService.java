package org.senseta.wiz.uipart;

import java.awt.Component;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.EventObject;
import java.util.List;

import javax.swing.Icon;
import javax.swing.JCheckBox;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.SwingConstants;
import javax.swing.event.CellEditorListener;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.TableCellEditor;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

import org.senseta.MaxService;

public class UI_MaxService implements Comparable<UI_MaxService>
{
	private MaxService service;
	
	private JCheckBox show, log;
	
	public UI_MaxService(MaxService service)
	{
		this.service = service;
		
		show = new JCheckBox();
		show.setOpaque(true);
		show.setHorizontalAlignment(SwingConstants.CENTER);
		show.setFocusPainted(false);
		show.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				
			}
		});
		
		log = new JCheckBox();
		log.setOpaque(true);
		log.setHorizontalAlignment(SwingConstants.CENTER);
		log.setFocusPainted(false);
		log.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				
			}
		});
	}
	
	@Override
	public int compareTo(UI_MaxService o)
	{
		return service.compareTo(o.service);
	}
	
	@Override
	public boolean equals(Object o)
	{
		if (!(o instanceof UI_MaxService))
			return false;
		
		return service.equals(((UI_MaxService)o).service);
	}
	
	public JCheckBox getShowCheckbox() { return show; }
	public JCheckBox getLogCheckbox() { return log; }
	public Icon getIcon() { return null; }
	

	public static void prepareColumn(TableColumn col, int i)
	{
		switch (i)
		{
			case 0:	col.setMaxWidth(35);		break;	//icon
			case 1: col.setPreferredWidth(125);	break;	//name
			case 2: col.setPreferredWidth(60);	break;	//format
			case 3: col.setPreferredWidth(225);	break;	//desc
			case 4: col.setMaxWidth(50);		break;	//show?
			case 5: col.setMaxWidth(50);		break;	//log?
		}
		
		col.setCellEditor(new ServiceCellEditor());
		col.setCellRenderer(new ServiceCellRenderer());
	}
	
	public static class ServiceTableModel extends AbstractTableModel
	{
		private static final long serialVersionUID = 0L;
		
		List<UI_MaxService> list;
		public ServiceTableModel(List<UI_MaxService> list)
		{
			this.list = list;
		}
		
		@Override public Class<?> getColumnClass(int columnIndex) { return UI_MaxService.class; }
		@Override public int getColumnCount() { return 6; }
		@Override public String getColumnName(int columnIndex) { return new String[]{"","Name", "Format", "Description", "Show?", "Log?"}[columnIndex]; }
		@Override public int getRowCount() { return list.size(); }
		@Override public Object getValueAt(int rowIndex, int columnIndex) { return list.get(rowIndex); }
		@Override public boolean isCellEditable(int row, int col) { return true; }
	}
	
	private static class ServiceCellEditor implements TableCellEditor
	{
		@Override public boolean stopCellEditing() { return true; }
		@Override public void cancelCellEditing() {}
		@Override public boolean shouldSelectCell(EventObject anEvent) { return true; }
		@Override public boolean isCellEditable(EventObject anEvent) { return true; }
		@Override public void removeCellEditorListener(CellEditorListener l) {}
		@Override public void addCellEditorListener(CellEditorListener l) {}
		@Override public Object getCellEditorValue() { return null; }
		
		@Override
		public Component getTableCellEditorComponent(JTable table, Object value, boolean isSelected, int row, int column)
		{
			UI_MaxService s = (UI_MaxService)value;
			switch (column)
			{
				case 4: return s.getShowCheckbox();
				case 5: return s.getLogCheckbox();
			
				default: return null;
			}
		}
	}
	
	private static class ServiceCellRenderer implements TableCellRenderer
	{
		private JLabel l;
		
		private ServiceCellRenderer()
		{
			l = new JLabel();
			l.setOpaque(true);
		}
		
		@Override
		public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column)
		{
			UI_MaxService s = (UI_MaxService)value;
			
			switch (column)
			{
				case 0: l.setIcon(s.getIcon());		break;
				case 1:	l.setText("Name");			break;
				case 2: l.setText("Format");		break;
				case 3:	l.setText("Description");	break;
				
				case 4: return s.getShowCheckbox();
				case 5: return s.getLogCheckbox();
			}
			
			return l;
		}
	}
}
