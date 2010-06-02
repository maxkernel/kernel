package org.webcontrol.applet.ui;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JScrollPane;

import org.webcontrol.applet.client.ControlClient;
import org.webcontrol.applet.client.ControlClient.SnapshotState;

public class VideoPanel extends JLabel
{
	private static final long serialVersionUID = 0L;
	
	private URL url;
	private URLConnection conn;
	private DataInputStream in;
	private ControlClient client;
	
	private byte[] data;
	private Object barrier = new Object();
	
	public VideoPanel(ControlClient client)
	{
		super("< No Video >", JLabel.CENTER);
		this.client = client;
		setOpaque(true);
		setBackground(Color.BLACK);
	}
	
	public void connect(URL videourl) throws IOException
	{
		setText("< No Video - "+videourl+" >");
		url = videourl;
		
		Thread receive = new Thread(new Runnable()
		{
			@Override
			public void run() {
				while (true)
				{
					try
					{
						conn = url.openConnection();
						in = new DataInputStream(new BufferedInputStream(conn.getInputStream()));
						
						while (true)
						{
							int size = in.readInt();
							
							byte[] data = new byte[size];
							readall(in, data);
							
							if (client.snapshot == SnapshotState.DISABLED)
							{
								VideoPanel.this.data = data;
								synchronized (barrier) {
									barrier.notifyAll();
								}	
							}
							else
							{
								BufferedImage img = ImageIO.read(new ByteArrayInputStream(data));
								if (client.snapshot == SnapshotState.SAVE)
								{
									JFrame frame = new JFrame("Snapshot");
									frame.setContentPane(new JScrollPane(new JLabel(new ImageIcon(img))));
									frame.setSize(500, 500);
									frame.setVisible(true);
									
									
									client.snapshot = SnapshotState.AFTER;
								}
								else if (client.snapshot == SnapshotState.THROWAWAY && img.getWidth() == 1600)
								{
									client.snapshot = SnapshotState.SAVE;
								}
								else if (client.snapshot == SnapshotState.AFTER && img.getWidth() == 160)
								{
									client.snapshot = SnapshotState.DISABLED;
								}
							}
							
						}
					} catch (IOException e)
					{
						e.printStackTrace();
					}
					
					try {
						in.close();
					} catch (Exception e){}
				}
			}
			
		});
		receive.setDaemon(true);
		receive.start();
		
		
		Thread render = new Thread(new Runnable()
		{
			@Override
			public void run() {
				while (true)
				{
					try
					{
						synchronized (barrier) {
							barrier.wait();
						}
						
					} catch (InterruptedException e)
					{
						e.printStackTrace();
					}
					
					try
					{
						
						BufferedImage image = ImageIO.read(new ByteArrayInputStream(data));
						
						setText(null);
						setIcon(new ImageIcon(image.getScaledInstance(getWidth(), (int)((double)getWidth()*0.75), BufferedImage.SCALE_DEFAULT)));
					
					} catch (IOException e)
					{
						e.printStackTrace();
					}
				}
			}
		});
		render.setDaemon(true);
		render.start();
	}
	
	private static void readall(InputStream in, byte[] b) throws IOException
	{
		int offset = 0;
		int numRead = 0;
		while (offset < b.length && (numRead= in.read(b, offset, b.length-offset)) >= 0) {
			offset += numRead;
		}
	}
}
