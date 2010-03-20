package org.senseta.wiz.ui;

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
import java.io.IOException;
import java.net.InetAddress;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.ListSelectionModel;
import javax.swing.SwingUtilities;
import javax.swing.border.EtchedBorder;
import javax.swing.event.AncestorEvent;
import javax.swing.event.AncestorListener;
import javax.swing.table.AbstractTableModel;

import org.senseta.MaxRobot;
import org.senseta.client.DiscoveryClient;
import org.senseta.event.BrowseListener;
import org.senseta.event.LayoutListener;
import org.senseta.wiz.ImageCache;
import org.senseta.wiz.Prompt;
import org.senseta.wiz.Prompt.Button;
import org.senseta.wiz.uipart.UI_MaxRobot;
import org.senseta.wiz.uipart.UI_MaxRobot.MaxRobotTableModel;

public class RobotBrowser extends JPanel implements LayoutManager, BrowseListener
{
	private static final Logger LOG = Logger.getLogger(RobotBrowser.class.getName());
	private static final long serialVersionUID = 0L;
	private static final Pattern IP_PATTERN = Pattern.compile("^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$");
	
	private static final int ROWHEIGHT = 50;
	
	private DiscoveryClient client;
	private MaxRobotTableModel browsemodel;
	
	private JTable browsetable;
	private JScrollPane scroll;
	
	private JLabel addlabel, discover;
	private JTextField add;
	private JButton addbutton, refresh;
	
	private Prompt prompt;
	
	public RobotBrowser()
	{
		prompt = null;
		
		client = new DiscoveryClient();
		client.addBrowseListener(this);

		browsemodel = new MaxRobotTableModel();
		
		browsetable = new JTable(browsemodel);
		browsetable.setRowHeight(ROWHEIGHT);
		browsetable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
		browsetable.setShowGrid(false);
		browsetable.setIntercellSpacing(new Dimension(0,0));
		
		for (int i=0; i<browsetable.getColumnCount(); i++)
		{
			UI_MaxRobot.prepareColumn(browsetable.getColumnModel().getColumn(i), i);
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
		
		scroll = new JScrollPane(browsetable, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		scroll.getViewport().setOpaque(false);
		scroll.setBorder(BorderFactory.createEtchedBorder(EtchedBorder.LOWERED));
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		addAncestorListener(new AncestorListener() {
			@Override
			public void ancestorRemoved(AncestorEvent event)
			{
				client.clearList();
			}
			
			@Override public void ancestorAdded(AncestorEvent event) {}
			@Override public void ancestorMoved(AncestorEvent event) {}
		});
		
		ActionListener addaction = new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				final String text = add.getText();
				if (text.length() == 0)
					return;
				
				Matcher m = IP_PATTERN.matcher(text);
				if (m.matches())
				{
					for (int i=1; i<=4; i++)
					{
						if (Integer.parseInt(m.group(i)) > 255)
						{
							Prompt p = Prompt.newErrorPrompt("Invalid IP address");
							p.addActionListener(new ActionListener() {
								@Override
								public void actionPerformed(ActionEvent e)
								{
									clearPrompt();
								}
							});
							showPrompt(p);
							return;
						}
					}
					
					try
					{
						InetAddress addr = InetAddress.getByName(text);
						MaxRobot r = null;
						if ((r = client.detectIP(text)) != null)
						{
							setSelected(r);
							add.setText("");
							browsetable.requestFocus();
						}
						else if (addr.isReachable(250))
						{
							r = MaxRobot.makeUnverified(text);
							robotDetected(r);
							setSelected(r);
							
							add.setText("");
							browsetable.requestFocus();
						}
						else
						{
							String msg = "Unable to confirm valid robot.\nAre you sure you want to add "+text+"?";
							final Prompt p = Prompt.newConfirmCancel(msg);
							p.addActionListener(new ActionListener() {
								@Override
								public void actionPerformed(ActionEvent e)
								{
									clearPrompt();
									if (p.getSelected() == Button.YES)
									{
										MaxRobot r = MaxRobot.makeUnverified(text);
										robotDetected(r);
										setSelected(r);
										
										add.setText("");
										browsetable.requestFocus();
									}
								}
							});
							showPrompt(p);
						}
					}
					catch (IOException ex)
					{
						LOG.log(Level.WARNING, "IOException occured while looking up host", ex);
						
						
						String msg = "Error occured while looking up host "+text+".\nReason"+ex.getMessage();
						Prompt p = Prompt.newErrorPrompt(msg);
						p.addActionListener(new ActionListener() {
							@Override
							public void actionPerformed(ActionEvent e)
							{
								clearPrompt();
							}
						});
						showPrompt(p);
					}
				}
				else
				{
					MaxRobot r = client.detectName(text);
					if (r == null)
					{
						String msg = "Could not detect "+text+". It does not seem to be on the network";
						Prompt p = Prompt.newErrorPrompt(msg);
						p.addActionListener(new ActionListener() {
							@Override
							public void actionPerformed(ActionEvent e)
							{
								clearPrompt();
							}
						});
						showPrompt(p);
					}
					else
					{
						setSelected(r);
						add.setText("");
						browsetable.requestFocus();
					}
				}
			}
		};
		
		addlabel = new JLabel("Name/IP:");
		add = new JTextField();
		add.addActionListener(addaction);
		
		addbutton = new JButton("Add", new ImageIcon(ImageCache.fromCache("sign_add")));
		addbutton.setToolTipText("Add to list");
		addbutton.addActionListener(addaction);
		
		
		discover = new JLabel("Discovered rovers:");
		refresh = new JButton(new ImageIcon(ImageCache.fromCache("refresh")));
		refresh.setToolTipText("Refresh list");
		refresh.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				client.clearList();
				client.broadcastNow();
			}
		});
		
		
		setLayout(this);
		add(addlabel);
		add(add);
		add(addbutton);
		add(discover);
		add(refresh);
		add(scroll);
	}
	
	@Override
	public void requestFocus()
	{
		browsetable.requestFocus();
	}
	
	public MaxRobot getSelected()
	{
		int row = browsetable.getSelectedRow();
		if (row != -1)
		{
			return ((UI_MaxRobot)browsemodel.getValueAt(row, 0)).getMaxRobot();
		}
		
		return null;
	}
	
	public void setSelected(MaxRobot robot)
	{
		if (robot == null)
		{
			//deselect everything
			browsetable.getSelectionModel().clearSelection();
			return;
		}
		
		boolean found = false;
		int index = 0;
		for (UI_MaxRobot r : browsemodel.getList())
		{
			if (robot.equals(r.getMaxRobot()))
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
	
	public void startBroadcastDaemon()
	{
		client.startBroadcastDaemon();
	}
	
	public void stopBroadcastDaemon()
	{
		client.stopBroadcastDaemon();
	}
	
	public void broadcastNow()
	{
		client.broadcastNow();
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

	@Override
	public void robotDetected(MaxRobot newrobot)
	{
		//preserve selected robot
		MaxRobot sel = getSelected();
		int selrow = browsetable.getSelectedRow();
		
		UI_MaxRobot uirobot = newrobot.getUIPart();
		
		//don't need to updateTable() too much
		if (browsemodel.getList().size() == 0)
		{
			uirobot.getPanel().addLayoutListener(new LayoutListener() {
				@Override
				public void layoutTriggered(JComponent component)
				{
					updateTable();
				}
			});
		}
		
		browsemodel.addRobot(uirobot);
		updateTable();
		
		//reselect the selected robot
		if (sel != null)
		{
			selrow += Math.max(-1, Math.min(1, sel.compareTo(newrobot)));
			browsetable.getSelectionModel().setSelectionInterval(0, selrow);
		}
	}

	@Override
	public void robotListCleared()
	{
		browsemodel.clearList();
		((AbstractTableModel)browsetable.getModel()).fireTableDataChanged();
	}
	
	private void showPrompt(Prompt p)
	{
		if (prompt != null)
		{
			clearPrompt();
		}
		
		prompt = p;
		add(prompt);
		p.requestFocus();
		revalidate();
	}
	
	private void clearPrompt()
	{
		if (prompt == null)
		{
			return;
		}
		
		remove(prompt);
		prompt = null;
		revalidate();
	}
	
	private void updateTable()
	{
		//preserve scroll position
		int posy = scroll.getViewport().getViewPosition().y;
		
		//preserve selected robot
		int selrow = browsetable.getSelectedRow();
		
		//update the table
		((AbstractTableModel)browsetable.getModel()).fireTableDataChanged();
		
		//reset the scroll position
		scroll.getViewport().setViewPosition(new Point(0, posy));
		
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
		
		discover.setBounds(x+5, y, w-80, 25);
		refresh.setBounds(w-25, y, 25, 25);
		addlabel.setBounds(x+5, h-25, 80, 25);
		add.setBounds(x+85, h-25, w-165, 25);
		addbutton.setBounds(w-80, h-25, 80, 25);
		
		if (prompt == null)
		{
			scroll.setBounds(x, y+25, w, h-51);
		}
		else
		{
			scroll.setBounds(x, y+25, w, h-151);
			prompt.setBounds(x, h-126, w, 100);
		}
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0, 0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
