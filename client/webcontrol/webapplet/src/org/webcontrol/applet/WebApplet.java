package org.webcontrol.applet;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.GraphicsDevice;
import java.awt.GraphicsEnvironment;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.net.URL;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JApplet;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JToggleButton;
import javax.swing.JWindow;

import org.webcontrol.applet.client.ControlClient;
import org.webcontrol.applet.joystick.Joystick;
import org.webcontrol.applet.ui.DriveStick;
import org.webcontrol.applet.ui.LookStick;
import org.webcontrol.applet.ui.VideoPanel;

public class WebApplet extends JApplet implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	private static final int JOYSTICK_POLL = 50;
	
	private JPanel content;
	
	private ControlClient control;
	private Joystick joystick;
	
	private VideoPanel videopanel;
	private DriveStick drive;
	private LookStick look;
	
	private JToggleButton usejoystick, takecontrol, fullscreen;
	private JButton snapshot;
	private JLabel logo, l_drive, l_look;
	
	public WebApplet() throws Exception
	{	
		drive = new DriveStick();
		look = new LookStick();
		
		control = new ControlClient(drive, look);
		videopanel = new VideoPanel(control);
		joystick = null;
		
		logo = new JLabel();
		logo.setHorizontalAlignment(JLabel.CENTER);
		logo.setVerticalAlignment(JLabel.BOTTOM);
		l_drive = new JLabel("Drive");
		l_look = new JLabel("Look");
		
		snapshot = new JButton("Snapshot");
		snapshot.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				control.takeSnapshot();
			}
		});
		
		usejoystick = new JToggleButton("Use Joystick");
		usejoystick.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				if (joystick == null)
				{
					joystick = Joystick.openJoystick();
					if (joystick == null || !joystick.isConnected())
					{
						if (fullscreen.isSelected())
							fullscreen.doClick();
						
						String err = joystick == null?
								"Error: Joystick interface library is not installed.\nSee downloads page." :
								"No Joystick found!\nPlug one in and press refresh";
						
						JOptionPane.showMessageDialog(getRootPane(), err, "Joystick Error", JOptionPane.ERROR_MESSAGE);
						usejoystick.setSelected(false);
						joystick = null;
					}
				}
				
				if (!usejoystick.isSelected())
				{
					drive.resetPosition();
				}
			}
		});
		
		takecontrol = new JToggleButton("Take Control");
		takecontrol.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				control.setHaveControl(takecontrol.isSelected());
			}
		});
		
		fullscreen = new JToggleButton("Fullscreen");
		fullscreen.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				GraphicsEnvironment ge = GraphicsEnvironment.getLocalGraphicsEnvironment();
				GraphicsDevice gs = ge.getDefaultScreenDevice();
				
				if (fullscreen.isSelected())
				{
					remove(content);
					
					JWindow window = new JWindow();
					window.setLayout(new BorderLayout());
					window.add(content, BorderLayout.CENTER);
					
					gs.setFullScreenWindow(window);
				}
				else
				{
					add(content, BorderLayout.CENTER);
					
					gs.setFullScreenWindow(null);
				}
			}
		});
		
		content = new JPanel();
		content.setLayout(this);
		content.add(videopanel);
		content.add(drive);
		content.add(look);
		content.add(snapshot);
		
		content.add(logo);
		content.add(l_look);
		content.add(l_drive);
		
		content.add(usejoystick);
		content.add(takecontrol);
		//content.add(fullscreen);
		
		setLayout(new BorderLayout());
		add(content, BorderLayout.CENTER);
	}
	
	@Override
	public void start()
	{
		try
		{
			URL base = getDocumentBase();
			videopanel.connect(new URL(base.getProtocol()+"://"+base.getHost()+":"+base.getPort()+"/video"));
			control.connect(new URL(base.getProtocol()+"://"+base.getHost()+":"+base.getPort()+"/control"));
			
			BufferedImage logo = ImageIO.read(new URL(base.getProtocol()+"://"+base.getHost()+":"+base.getPort()+"/nasa.png").openConnection().getInputStream());
			this.logo.setIcon(new ImageIcon(logo));
			
		} catch (IOException e)
		{
			e.printStackTrace();
		}
		
		//schedule the joystick poller
		ScheduledExecutorService es = Executors.newSingleThreadScheduledExecutor(new ThreadFactory() {
			@Override
			public Thread newThread(Runnable r) {
				Thread t = new Thread(r);
				t.setDaemon(true);
				return t;
			}
		});
		es.scheduleAtFixedRate(new Runnable() {
			@Override public void run() { polljoystick(); }
		}, JOYSTICK_POLL, JOYSTICK_POLL, TimeUnit.MILLISECONDS);
	}
	
	@Override
	public void stop()
	{
		control.setHaveControl(false);
	}
	
	private void polljoystick()
	{
		if (joystick == null || !usejoystick.isSelected())
			return;
		
		if (!joystick.poll())
		{
			usejoystick.setSelected(false);
			drive.resetPosition();
			joystick = null;
			return;
		}
		
		drive.updateWithJoystick(joystick);
		look.updateWithJoystick(joystick);
	}
	
	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		videopanel.setBounds(x, y, w-350, h);
		
		l_drive.setBounds(w-345, 25, 100, 25);
		drive.setBounds(w-345, y+50, 175, 175);
		snapshot.setBounds(w-150, y+50, 125, 30);
		
		l_look.setBounds(w-345, y+250, 100, 25);
		look.setBounds(w-345, y+275, 340, 125);
		
		usejoystick.setBounds(w-345, y+410, 115, 25);
		takecontrol.setBounds(w-225, y+410, 115, 25);
		//fullscreen.setBounds(w-105, y+410, 100, 25);
		
		logo.setBounds(w-350, y+450, 350, h-460);
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
