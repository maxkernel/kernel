package org.senseta;

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

import org.senseta.wiz.uipart.UI_MaxService;

public class MaxService implements Comparable<MaxService>
{
	private String name, format, desc;
	private int port;
	
	
	public MaxService()
	{
		
	}
	
	public UI_MaxService getUIPart()
	{
		return new UI_MaxService(this);
	}
	
	@Override
	public int compareTo(MaxService o)
	{
		return name.compareTo(o.name);
	}

	
}
