package org.senseta.wiz.ui;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyAdapter;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.io.File;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.JTable;
import javax.swing.JTextArea;
import javax.swing.ListSelectionModel;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;
import javax.swing.border.EtchedBorder;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.filechooser.FileFilter;
import javax.swing.table.AbstractTableModel;

import org.senseta.SoftwareBundle;
import org.senseta.SoftwareCache;
import org.senseta.SoftwareBundle.Type;
import org.senseta.wiz.ImageCache;
import org.senseta.wiz.Prompt;
import org.senseta.wiz.Prompt.Button;
import org.senseta.wiz.uipart.UI_SoftwareBundle;
import org.senseta.wiz.uipart.UI_SoftwareBundle.SoftwareBundleTableModel;

public class SoftwareBrowser extends JSplitPane implements LayoutManager
{
	private static final Logger LOG = Logger.getLogger(SoftwareBrowser.class.getName());
	private static final long serialVersionUID = 0L;
	
	private static final int ROWHEIGHT = 30;
	
	private JTable browsetable;
	private JScrollPane browse_scroll;
	private SoftwareBundleTableModel browsemodel;
	
	private JPanel top;
	private JButton add, remove;
	private JTextArea info;
	private JScrollPane info_scroll;
	private Prompt prompt;
	
	public SoftwareBrowser()
	{
		super(JSplitPane.VERTICAL_SPLIT);
		setDividerLocation(250);
		setResizeWeight(1.0);
		setContinuousLayout(true);
		
		prompt = null;
		
		browsemodel = new SoftwareBundleTableModel(SoftwareCache.getCache());
		
		browsetable = new JTable(browsemodel);
		browsetable.setRowHeight(ROWHEIGHT);
		browsetable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
		browsetable.setShowGrid(false);
		browsetable.setIntercellSpacing(new Dimension(0,0));
		
		for (int i=0; i<browsetable.getColumnCount(); i++)
		{
			UI_SoftwareBundle.prepareColumn(browsetable.getColumnModel().getColumn(i), i);
		}
		
		browsetable.addMouseListener(new MouseAdapter() {
			@Override
			public void mouseClicked(MouseEvent e)
			{
				if (e.getClickCount() == 2)
				{
					ActionEvent ev = new ActionEvent(e.getSource(), e.getID(), e.paramString());
					fireActionEvent(ev);
				}
			}
		});
		
		browsetable.addKeyListener(new KeyAdapter() {
			@Override
			public void keyTyped(KeyEvent e)
			{
				if (e.getKeyChar() == 10) //enter pressed
				{
					ActionEvent ev = new ActionEvent(e.getSource(), e.getID(), e.paramString());
					fireActionEvent(ev);
				}
			}
		});
		
		browsetable.getSelectionModel().addListSelectionListener(new ListSelectionListener() {
			@Override
			public void valueChanged(ListSelectionEvent e)
			{
				setDetails(getSelected());
			}
		});
		
		browse_scroll = new JScrollPane(browsetable, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		browse_scroll.getViewport().setOpaque(false);
		browse_scroll.setBorder(BorderFactory.createEtchedBorder(EtchedBorder.LOWERED));
		browse_scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		info = new JTextArea();
		info.setBackground(new Color(0xEBEBEB));
		info.setEditable(false);
		
		add = new JButton("Add", new ImageIcon(ImageCache.fromCache("sign_add")));
		add.setHorizontalAlignment(SwingConstants.LEFT);
		add.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				JFileChooser chooser = new JFileChooser();
				chooser.setMultiSelectionEnabled(false);
				chooser.setFileFilter(new FileFilter() {
					@Override public String getDescription() { return "Software Bundle (*.stuff)"; }
					@Override public boolean accept(File f) { return f.isDirectory() || f.getName().endsWith(".stuff"); }
				});
				
				if (chooser.showOpenDialog(SoftwareBrowser.this) == JFileChooser.APPROVE_OPTION)
				{
					final File fp = chooser.getSelectedFile();
					final Runnable addfunc = new Runnable() {
						@Override
						public void run()
						{
							SoftwareBundle bundle = null;
							
							try
							{
								bundle = SoftwareCache.add(fp);
							}
							catch (Exception e)
							{
								Prompt p = Prompt.newMessage("Error while adding software bundle.\nReason: "+e.getMessage(), Prompt.ICON_ERROR);
								p.addActionListener(new ActionListener() {
									@Override
									public void actionPerformed(ActionEvent e)
									{
										clearPrompt();
									}
								});
								showPrompt(p);
							}
							
							browsemodel.addBundle(bundle);
							updateTable();
							setSelected(bundle);
						}
					};
					
					if (SoftwareCache.exists(fp))
					{
						final Prompt p = Prompt.newConfirmCancel("Overwrite cache version "+fp.getName()+"?");
						p.addActionListener(new ActionListener() {
							@Override
							public void actionPerformed(ActionEvent e)
							{
								clearPrompt();
								if (p.getSelected() == Button.YES)
								{
									addfunc.run();
								}
							}
						});
						showPrompt(p);
					}
					else
					{
						addfunc.run();
					}
				}
			}
		});
		
		remove = new JButton("Remove", new ImageIcon(ImageCache.fromCache("sign_remove")));
		remove.setHorizontalAlignment(SwingConstants.LEFT);
		remove.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				final SoftwareBundle bundle = getSelected();
				if (bundle == null)
				{
					return;
				}
				
				final Prompt p = Prompt.newConfirmCancel("Are you sure you want to remove "+bundle.getName()+"?");
				p.addActionListener(new ActionListener() {
					@Override
					public void actionPerformed(ActionEvent e)
					{
						if (p.getSelected() == Button.YES)
						{
							SoftwareCache.remove(bundle);
							browsemodel.removeBundle(bundle);
							setSelected(null);
							updateTable();
						}
						
						clearPrompt();
					}
				});
				showPrompt(p);
			}
		});
		
		top = new JPanel();
		top.setLayout(this);
		top.add(browse_scroll);
		top.add(add);
		top.add(remove);
		
		info_scroll = new JScrollPane(info, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		info_scroll.setBorder(BorderFactory.createEmptyBorder());
		info_scroll.setViewportBorder(BorderFactory.createTitledBorder("Details"));
		
		setTopComponent(top);
		setBottomComponent(info_scroll);
	}
	
	@Override
	public void requestFocus()
	{
		browsetable.requestFocus();
	}
	
	public void addActionListener(ActionListener l)
	{
		listenerList.add(ActionListener.class, l);
	}
	
	public void removeActionListener(ActionListener l)
	{
		listenerList.remove(ActionListener.class, l);
	}
	
	private void fireActionEvent(ActionEvent e)
	{
		Object[] list = listenerList.getListenerList();
		for (int i=list.length-2; i>=0; i-=2)
		{
			if (list[i] == ActionListener.class)
			{
				try {
					((ActionListener)list[i+1]).actionPerformed(e);
				}
				catch (Throwable t)
				{
					LOG.log(Level.WARNING, "Event listener (ActionListener) threw an exception", t);
				}
			}
		}
	}
	
	public SoftwareBundle getSelected()
	{
		int row = browsetable.getSelectedRow();
		if (row != -1)
		{
			return ((UI_SoftwareBundle)browsemodel.getValueAt(row, 0)).getSoftwareBundle();
		}
		
		return null;
	}
	
	public void setSelected(SoftwareBundle bundle)
	{
		if (bundle == null)
		{
			//deselect everything
			browsetable.getSelectionModel().clearSelection();
			return;
		}
		
		boolean found = false;
		int index = 0;
		for (UI_SoftwareBundle b : browsemodel.getList())
		{
			if (bundle.equals(b.getSoftwareBundle()))
			{
				found = true;
				break;
			}
			
			index++;
		}
		
		if (found)
		{
			browsetable.getSelectionModel().setSelectionInterval(0, index);
			
			final int offset = index*ROWHEIGHT;
			SwingUtilities.invokeLater(new Runnable() {
				@Override
				public void run()
				{
					browsetable.scrollRectToVisible(new Rectangle(0, offset, 1, ROWHEIGHT));
				}
			});
		}
	}
	
	private void showPrompt(Prompt p)
	{
		if (prompt != null)
		{
			clearPrompt();
		}
		
		prompt = p;
		top.add(prompt);
		p.requestFocus();
		top.revalidate();
	}
	
	private void clearPrompt()
	{
		if (prompt == null)
		{
			return;
		}
		
		top.remove(prompt);
		prompt = null;
		top.revalidate();
	}
	
	private void setDetails(SoftwareBundle b)
	{
		if (b == null)
		{
			info.setText("");
			return;
		}
		
		String desc = b.getDescription();
		if (desc == null)
			desc = "(no description)";
		
		if (b.getType() == Type.UPGRADE)
			desc = "*Upgrade*  "+desc;
		
		if (b.getAuthor() != null)
			desc += "\nAuthor: " + b.getAuthor();
		
		if (b.getProvider() != null)
			desc += "\nProvider: "+b.getProvider();
		
		
		info.setText(desc);
		info.setCaretPosition(0);
	}
	
	private void updateTable()
	{
		//preserve scroll position
		int posy = browse_scroll.getViewport().getViewPosition().y;
		
		//preserve selected robot
		int selrow = browsetable.getSelectedRow();
		
		//update the table
		((AbstractTableModel)browsetable.getModel()).fireTableDataChanged();
		
		//reset the scroll position
		browse_scroll.getViewport().setViewPosition(new Point(0, posy));
		
		//reselect the selected robot
		if (selrow != -1)
		{
			browsetable.getSelectionModel().setSelectionInterval(0, selrow);
		}
	}
	
	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		add.setBounds(w-95, y+25, 95, 30);
		remove.setBounds(w-95, y+55, 95, 30);
		
		if (prompt == null)
		{
			browse_scroll.setBounds(x, y, w-95, h);
		}
		else
		{
			browse_scroll.setBounds(x, y, w-95, h-100);
			prompt.setBounds(x, h-100, w-95, 100);
		}
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
