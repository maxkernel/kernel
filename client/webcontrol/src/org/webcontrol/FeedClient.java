package org.webcontrol;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;

import org.webcontrol.handler.VideoHandler;

public class FeedClient implements Runnable
{
	private static final int FEEDPORT = 8089;
	private static final int TIMEOUT = 4000;
	private static final DateFormat DATEFMT = new SimpleDateFormat("yyyy-MM-dd'T'HH-mm-ss-SSS");
	private static final File CAPTUREDIR = new File("/var/capture");
	
	@Override
	public void run()
	{
		try
		{
			ServerSocket feedserver = new ServerSocket(FEEDPORT, 0);
			while (true)
			{
				Socket fclient = feedserver.accept();
				fclient.setSoTimeout(TIMEOUT);
				BufferedInputStream in = new BufferedInputStream(fclient.getInputStream());
				
				try
				{
					while (true)
					{
						byte[] header = new byte[4];
						readall(in, header);
						
						int size = 0;
						size |= ((int)header[3] << 24) & 0xFF000000;
						size |= ((int)header[2] << 16) & 0x00FF0000;
						size |= ((int)header[1] << 8)  & 0x0000FF00;
						size |= ((int)header[0])       & 0x000000FF;
						size -= 4;
						
						if (size <= 0)
						{
							break;
						}
						
						byte[] data = new byte[size];
						readall(in, data);
						
						VideoHandler.DATA = data;
						synchronized (VideoHandler.BARRIER) {
							VideoHandler.BARRIER.notifyAll();
						}
						
						/*
						if (CAPTUREDIR.exists())
						{
							File fp = new File(CAPTUREDIR, DATEFMT.format(new Date())+".jpg");
							FileOutputStream fout = new FileOutputStream(fp);
							fout.write(data);
							fout.flush();
							fout.close();
						}
						*/
					}
					
				} catch (Exception e)
				{
					//ignore
				}
				
				try { in.close(); } catch (Exception e){}
				try { fclient.close(); } catch (Exception e){}
				
				
				VideoHandler.DATA = VideoHandler.NOIMG;
				System.gc();
			}
			
		} catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	private void readall(InputStream in, byte[] b) throws IOException
	{
		int offset = 0;
		int numRead = 0;
		while (offset < b.length && (numRead= in.read(b, offset, b.length-offset)) >= 0) {
			offset += numRead;
		}
	}
}
