package org.senseta.client;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;

public class SoftwareBundleClient
{
	private static final int DEPLOY_PORT = 59001;
	private static final int TIMEOUT = 3000;
	
	private Socket socket;
	private BufferedInputStream in;
	private OutputStream out;
	
	public SoftwareBundleClient(String ip) throws IOException
	{
		socket = new Socket();
		socket.connect(new InetSocketAddress(ip, DEPLOY_PORT), TIMEOUT);
		in = new BufferedInputStream(socket.getInputStream());
		out = socket.getOutputStream();
	}
	
	public SoftwareBundleClient(MaxRobot robot) throws IOException
	{
		this(robot.getIP());
	}
	
	public SoftwareBundle getRunningBundle() throws IOException, IllegalArgumentException
	{
		out.write("running".getBytes());
		out.flush();
		
		return SoftwareBundle.fromServerStream(in);
	}
	
	public File downloadBundle() throws IOException, IllegalArgumentException
	{	
		File fp = File.createTempFile("bundle", ".stuff");
		fp.deleteOnExit();
		
		FileOutputStream fout = new FileOutputStream(fp);
		
		byte[] buf = new byte[1024];
		int read;
		
		out.write("bundle".getBytes());
		out.flush();
		
		do
		{
			read = in.read(buf);
			if (read > 0)
			{
				fout.write(buf, 0, read);
			}
		}
		while (read > 0);
		fout.flush();
		fout.close();
		
		return fp;
	}
	
	public void close()
	{
		try { out.flush(); out.close(); } catch (IOException e) {}
		try { in.close(); } catch (IOException e) {}
		try { socket.close(); } catch (IOException e) {}
	}
}
