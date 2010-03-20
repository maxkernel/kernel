package org.senseta.wiz;

import java.awt.Dimension;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.ArrayList;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

import org.senseta.MaxService;
import org.senseta.client.ServiceClient;
import org.senseta.client.ServiceClient.Protocol;
import org.senseta.event.ServiceListener;
import org.senseta.wiz.Wizard.ViewPolicy;
import org.senseta.wiz.pages.BrowsePage;
import org.senseta.wiz.pages.RunFinalPage;
import org.senseta.wiz.pages.ServiceConfigPage;
import org.senseta.wiz.pages.ServiceFinalPage;
import org.senseta.wiz.pages.SoftwarePage;
import org.senseta.wiz.pages.WizardPagePart;
import org.senseta.wiz.ui.ServiceBrowser;

public class ServiceWizard
{
	
	public static void main(String[] args)
	{
		/*
		JFrame f = new JFrame();
		final JLabel l = new JLabel();
		f.setContentPane(l);
		f.setSize(320, 240);
		f.setVisible(true);
		f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		
		ServiceClient c = new ServiceClient(InetAddress.getByName("localhost"));
		c.openStream(Protocol.TCP, 10001);
		
		c.startKeepaliveThread(Protocol.TCP);
		c.startReadThread();
		c.startStream("video", Protocol.TCP, new ServiceListener() {
			@Override
			public void newFrame(String service, Protocol p, long timestampUs, byte[] data) {
				try {
					BufferedImage img = ImageIO.read(new ByteArrayInputStream(data));
					l.setIcon(new ImageIcon(img));
					
				} catch (IOException e)
				{
					e.printStackTrace();
				}
			}
		});
		
		Thread.sleep(10000);
		
		/*
		//set default l&f
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (Exception e) {} //ignore
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run()
			{
				JFrame frame = new JFrame("Service Wizard");
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				frame.setContentPane(new ServiceBrowser(new ArrayList<MaxService>()));
				
				frame.pack();
				frame.setSize(550, 400);
				frame.setMinimumSize(new Dimension(300, 200));
				frame.setLocation(0, 25);
				frame.setVisible(true);
			}
		});
		*/
		
		//set default l&f
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (Exception e) {} //ignore
		
		final WizardPagePart[] pages = {
			new BrowsePage(false),
			new ServiceConfigPage(),
			new ServiceFinalPage()
		};
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run()
			{
				JFrame frame = new Wizard("Service Wizard", pages, ViewPolicy.SIMPLE);
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				
				frame.pack();
				frame.setSize(400, 500);
				frame.setLocationRelativeTo(null);
				frame.setVisible(true);
			}
		});
	}
}
