package org.senseta.wiz.ui;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Toolkit;
import java.awt.datatransfer.StringSelection;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JDialog;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTabbedPane;
import javax.swing.JTextArea;
import javax.swing.border.EtchedBorder;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;
import org.senseta.SoftwareCache;
import org.senseta.wiz.ImageCache;
import org.senseta.wiz.uipart.UI_MaxRobot;

public class ProcessStarter extends JFrame implements WindowListener, LayoutManager, Runnable
{
	private static final long serialVersionUID = 0L;
	
	private UI_MaxRobot robotlabel;
	
	private JButton save, copy, export, info, exit;
	private JLabel bundleicon, roboticon;
	private JPanel robotpanel;
	private JTextArea output;
	private JScrollPane scroll;
	
	private Process process;
	private MaxRobot robot;
	private SoftwareBundle bundle;
	
	private ProcessStarter(ProcessBuilder builder, MaxRobot robot, SoftwareBundle bundle) throws IOException
	{
		super(bundle.getName() + " v" + Double.toString(bundle.getVersion()));
		setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);
		addWindowListener(this);
		
		this.robot = robot;
		this.bundle = bundle;
		
		if (robot != null)
		{
			robotlabel = robot.getUIPart();
			robotpanel = robotlabel.getPanel();
		}
		else
		{
			robotlabel = new UI_MaxRobot();
			robotpanel = new JPanel(new BorderLayout());
			
			JLabel l = new JLabel("Unknown robot");
			robotpanel.add(l, BorderLayout.CENTER);
		}
		
		bundleicon = new JLabel(new ImageIcon(ImageCache.scale(SoftwareCache.getIcon(bundle), 40, 40)));
		roboticon = new JLabel(new ImageIcon(ImageCache.scale(robotlabel.getIcon(), 40, 40)));
		
		save = new JButton(new ImageIcon(ImageCache.fromCache("save")));
		save.setToolTipText("Save output");
		save.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				JFileChooser chooser = new JFileChooser();
				chooser.setMultiSelectionEnabled(false);
				if (chooser.showOpenDialog(ProcessStarter.this) == JFileChooser.APPROVE_OPTION)
				{
					File fp = chooser.getSelectedFile();
					if (fp.exists() && JOptionPane.showConfirmDialog(ProcessStarter.this, "Are you sure you want to overwrite "+fp.getName()+"?", "Overwrite?", JOptionPane.YES_NO_OPTION, JOptionPane.QUESTION_MESSAGE) == JOptionPane.YES_OPTION)
						fp.delete();
					
					try {
						
						FileOutputStream fout = new FileOutputStream(fp);
						fout.write(output.getText().getBytes());
						fout.flush();
						fout.close();
						
					} catch (Exception ex) {
						String message = "Could not save file\nReason: "+ex.getMessage();
						JOptionPane.showMessageDialog(ProcessStarter.this, message, "Error", JOptionPane.ERROR_MESSAGE);
					}
				}
			}
		});
		
		copy = new JButton(new ImageIcon(ImageCache.fromCache("copy")));
		copy.setToolTipText("Copy console output");
		copy.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				Toolkit.getDefaultToolkit().getSystemClipboard().setContents(new StringSelection(output.getText()), null);
				output.requestFocus();
				output.selectAll();
			}
		});
		
		export = new JButton(new ImageIcon(ImageCache.fromCache("export")));
		export.setToolTipText("Export Auto-starter");
		export.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				// TODO Auto-generated method stub
				
			}
		});
		
		info = new JButton(new ImageIcon(ImageCache.fromCache("sign_info")));
		info.setToolTipText("Information");
		info.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				showInfoPane();
			}
		});
		
		exit = new JButton(new ImageIcon(ImageCache.fromCache("sign_cancel")));
		exit.setToolTipText("Exit");
		exit.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				killprocess();
			}
		});
		
		output = new JTextArea("Output:\n");
		output.setFont(new Font("sans-serif", Font.PLAIN, 10));
		output.setEditable(false);
		output.setLineWrap(true);
		output.setWrapStyleWord(true);
		
		scroll = new JScrollPane(output, JScrollPane.VERTICAL_SCROLLBAR_ALWAYS, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		output.setBorder(BorderFactory.createEmptyBorder(3, 5, 3, 3));
		scroll.setBorder(BorderFactory.createEtchedBorder(EtchedBorder.LOWERED));
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		setLayout(this);
		add(bundleicon);
		add(roboticon);
		add(robotpanel);
		add(copy);
		add(save);
		add(export);
		add(info);
		add(exit);
		add(scroll);
		
		Thread t = new Thread(this);
		t.setDaemon(true);
		process = builder.start();
		t.start();
	}
	
	@Override
	public void run()
	{
		try {
			BufferedInputStream in = new BufferedInputStream(process.getInputStream());
			
			byte[] buf = new byte[1024];
			while (true)
			{
				int read = in.read(buf);
				if (read < 0)
				{
					append("\n** Groundstation application has closed");
					try {
						process.waitFor();
						append(" with status "+process.exitValue());
					} catch (InterruptedException e) {} //ignore
					
					break;
				}
				
				append(new String(buf, 0, read));
			}
		} catch (IOException e) {
			append("\n** Error: IOException - "+e.getMessage());
		}
		
	}
	
	private void killprocess()
	{
		process.destroy();
		System.exit(0);
	}
	
	private void append(String txt)
	{
		output.append(txt);
		output.setCaretPosition(output.getText().length());
	}
	
	private void showInfoPane()
	{
		JTabbedPane panel = new JTabbedPane();
		panel.add("Software", new SoftwarePane());
		panel.add("Hardware", new HardwarePane());
		
		JDialog dialog = new JDialog(this, "Information");
		dialog.getContentPane().setLayout(new BorderLayout());
		dialog.getContentPane().add(panel, BorderLayout.CENTER);
		dialog.getContentPane().add(new ButtonBar(dialog), BorderLayout.SOUTH);
		
		dialog.setSize(350, 350);
		dialog.setResizable(false);
		dialog.setLocation(35,35);
		dialog.setVisible(true);
	}
	
	@Override
	public void windowClosing(WindowEvent e)
	{
		killprocess();
	}
	
	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		bundleicon.setBounds(x, y, 50, 50);
		roboticon.setBounds(x+50, y, 50, 50);
		robotpanel.setBounds(x+100, y, w-200, 50);
		copy.setBounds(w-75, y+25, 25, 25);
		save.setBounds(w-50, y+25, 25, 25);
		export.setBounds(w-25, y+25, 25, 25);
		info.setBounds(x, h-50, 25, 25);
		exit.setBounds(x, h-25, 25, 25);
		scroll.setBounds(x+25, y+50, w-25, h-50);
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}

	@Override public void windowClosed(WindowEvent e) {}
	@Override public void windowActivated(WindowEvent e) {}
	@Override public void windowDeactivated(WindowEvent e) {}
	@Override public void windowDeiconified(WindowEvent e) {}
	@Override public void windowIconified(WindowEvent e) {}
	@Override public void windowOpened(WindowEvent e) {}
	
	private class SoftwarePane extends JPanel implements LayoutManager
	{
		private static final long serialVersionUID = 0L;
		
		private SoftwarePane()
		{
			
		}
		
		@Override
		public void layoutContainer(Container parent)
		{
			Insets in = parent.getInsets();
			int x = in.left, y = in.top;
			int w = parent.getWidth() - in.left - in.right;
			int h = parent.getHeight() - in.top - in.bottom;
			
			
		}

		@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
		@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
	}
	
	private class HardwarePane extends JPanel implements LayoutManager
	{
		private static final long serialVersionUID = 0L;
		
		private JLabel image;
		
		private HardwarePane()
		{
			image = new JLabel(new ImageIcon(ImageCache.scale(robotlabel.getIcon(), 100, 100)));
			image.setHorizontalAlignment(JLabel.CENTER);
			image.setVerticalAlignment(JLabel.BOTTOM);
			
			setLayout(this);
			add(image);
		}
		
		@Override
		public void layoutContainer(Container parent)
		{
			Insets in = parent.getInsets();
			int x = in.left, y = in.top;
			int w = parent.getWidth() - in.left - in.right;
			int h = parent.getHeight() - in.top - in.bottom;
			
			image.setBounds(x, y, w, 100);
		}

		@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
		@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
	}
	
	private static class ButtonBar extends JPanel implements LayoutManager
	{
		private static final long serialVersionUID = 0L;
		private JButton okay;
		
		private ButtonBar(final JDialog dialog)
		{
			okay = new JButton("Okay", new ImageIcon(ImageCache.fromCache("sign_tick")));
			okay.addActionListener(new ActionListener() {
				@Override
				public void actionPerformed(ActionEvent e)
				{
					dialog.dispose();
				}
			});
			
			setLayout(this);
			add(okay);
		}
		
		@Override
		public void layoutContainer(Container parent)
		{
			Insets in = parent.getInsets();
			int w = parent.getWidth() - in.left - in.right;
			int h = parent.getHeight() - in.top - in.bottom;
			
			okay.setBounds(w-95, h-35, 90, 30);
		}
		
		@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(parent.getWidth(), 40); }
		@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
	}
	
	public static void start(ProcessBuilder builder, MaxRobot robot, SoftwareBundle bundle) throws IOException
	{
		ProcessStarter frame = new ProcessStarter(builder, robot, bundle);
		int height = Toolkit.getDefaultToolkit().getScreenSize().height;
		
		frame.pack();
		frame.setSize(550, 170);
		frame.setMinimumSize(new Dimension(400, 100));
		frame.setLocation(0, height-170);
		frame.setVisible(true);
	}
}
