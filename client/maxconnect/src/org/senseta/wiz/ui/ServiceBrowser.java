package org.senseta.wiz.ui;

import java.awt.BorderLayout;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.imageio.ImageIO;
import javax.swing.BorderFactory;
import javax.swing.DefaultListSelectionModel;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JToolBar;
import javax.swing.ListSelectionModel;
import javax.swing.border.EtchedBorder;

import org.senseta.MaxRobot;
import org.senseta.MaxService;
import org.senseta.wiz.uipart.UI_MaxService;

public class ServiceBrowser extends JPanel
{
	private static final long serialVersionUID = 0L;
	
	private static Icon SAVE, LOG;
	private List<UI_MaxService> ui_services;
	
	static
	{
		try {
			SAVE = new ImageIcon(ImageIO.read(ServiceBrowser.class.getResourceAsStream("/org/senseta/img/save.png")));
			LOG = new ImageIcon(ImageIO.read(MaxRobot.class.getResourceAsStream("/org/senseta/img/log.png")));
		}
		catch (Exception e)
		{
			SAVE = LOG = new ImageIcon();
		}
	}
	
	private JTable servicetable;
	private JToolBar toolbar;
	private JScrollPane scroll;
	
	public ServiceBrowser(List<MaxService> services)
	{
		setOpaque(true);
		
		ArrayList<UI_MaxService> list = new ArrayList<UI_MaxService>();
		for (MaxService s : services)
		{
			list.add(s.getUIPart());
		}
		Collections.sort(list);
		ui_services = Collections.unmodifiableList(list);
		
		ListSelectionModel selectionmodel = new DefaultListSelectionModel() {
			private static final long serialVersionUID = 0L;
			@Override public boolean isSelectedIndex(int index) { return false; }
		};
		
		
		setLayout(new BorderLayout());
		
		toolbar = new JToolBar();
		
		JButton save = new JButton(SAVE);
		save.setFocusPainted(false);
		save.setToolTipText("Save layout");
		
		JButton setlog = new JButton(LOG);
		setlog.setFocusPainted(false);
		setlog.setToolTipText("Log settings");
		
		toolbar.add(save);
		toolbar.add(setlog);
		add(toolbar, BorderLayout.PAGE_START);
		
		
		servicetable = new JTable(new UI_MaxService.ServiceTableModel(ui_services));
		servicetable.setRowHeight(25);
		servicetable.setSelectionModel(selectionmodel);
		servicetable.setShowGrid(false);
		
		for (int i=0; i<servicetable.getColumnCount(); i++)
		{
			UI_MaxService.prepareColumn(servicetable.getColumnModel().getColumn(i), i);
		}

		scroll = new JScrollPane(servicetable, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		scroll.getViewport().setOpaque(false);
		scroll.setBorder(BorderFactory.createEtchedBorder(EtchedBorder.LOWERED));
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		add(scroll, BorderLayout.CENTER);
	}
}
