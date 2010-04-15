package org.webcontrol.handler;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import org.webcontrol.HttpCodes;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;

public class VideoHandler implements HttpHandler {
	
	public static Object BARRIER = new Object();
	private static final int BARRIER_WAIT = 4000;
	public static final byte[] NOIMG = loadimage("www/noimg.jpg");
	public static volatile byte[] DATA = NOIMG;
	
	@Override
	public void handle(HttpExchange ex) throws IOException {
		if (!ex.getRequestMethod().equalsIgnoreCase("GET"))
			return;
		
		//ex.getRequestURI();
		ex.sendResponseHeaders(HttpCodes.HTTP_OKAY, 0);
		DataOutputStream out = new DataOutputStream(ex.getResponseBody());
		
		try
		{
			do
			{
				byte[] data = DATA;
				
				out.writeInt(data.length);
				out.write(data);
				out.flush();
				
				synchronized (BARRIER) {
					BARRIER.wait(BARRIER_WAIT);
				}
			} while (true);
		} catch (InterruptedException e)
		{
			e.printStackTrace();
		} catch (IOException e)
		{
			//ignore
		}
		
		ex.close();
	}
	
	private static byte[] loadimage(String path)
	{
		File fp = new File(path);
		long len = fp.length();
		byte[] data = new byte[(int)len];

		try
		{
			FileInputStream fin = new FileInputStream(fp);

			int offset = 0;
			int numRead = 0;
			while (offset < data.length && (numRead= fin.read(data, offset, data.length-offset)) >= 0) {
				offset += numRead;
			}

			fin.close();
		} catch (IOException e)
		{
			System.out.println("Could not read in "+path+" image");
			e.printStackTrace();
		}
		
		return data;
	}
}
