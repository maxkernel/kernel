package org.webcontrol.applet;

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
import javax.swing.JLabel;

public class VideoPanel extends JLabel implements Runnable
{
	private static final long serialVersionUID = 0L;
	
	private URLConnection conn;
	private DataInputStream in;
	
	public VideoPanel()
	{
		super("< No Video >", JLabel.CENTER);
	}
	
	public void connect(URL videourl) throws IOException
	{
		setText("< No Video - "+videourl+" >");
		
		conn = videourl.openConnection();
		in = new DataInputStream(new BufferedInputStream(conn.getInputStream()));
		
		Thread t = new Thread(this);
		t.setDaemon(true);
		t.start();
	}

	@Override
	public void run() {
		try
		{
			while (true)
			{
				int size = in.readInt();
				
				byte[] data = new byte[size];
				readall(in, data);
				
				BufferedImage image = ImageIO.read(new ByteArrayInputStream(data));
				setText(null);
				setIcon(new ImageIcon(image));
			}
		} catch (IOException e)
		{
			e.printStackTrace();
		}
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
