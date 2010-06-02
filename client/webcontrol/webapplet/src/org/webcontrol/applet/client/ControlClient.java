package org.webcontrol.applet.client;

import java.io.DataOutputStream;
import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

import org.webcontrol.applet.ui.DriveStick;
import org.webcontrol.applet.ui.LookStick;

public class ControlClient
{
	private static final int TIMEOUT = 50;
	public static enum SnapshotState { DISABLED, THROWAWAY, SAVE, AFTER };
	public SnapshotState snapshot;
	
	private DriveStick drive;
	private LookStick look;
	
	
	private URL url;
	private boolean havecontrol;
	
	public ControlClient(DriveStick drive, LookStick look)
	{
		havecontrol = false;
		snapshot = SnapshotState.DISABLED;
		this.drive = drive;
		this.look = look;
	}
	
	public void connect(URL controlurl) throws IOException
	{
		url = controlurl;		
		ScheduledExecutorService es = Executors.newSingleThreadScheduledExecutor(new ThreadFactory() {
			@Override
			public Thread newThread(Runnable r) {
				Thread t = new Thread(r);
				t.setDaemon(true);
				return t;
			}
		});
		
		es.scheduleAtFixedRate(new Runnable() {
			@Override public void run() { sendcontrol(); }
		}, 0, TIMEOUT, TimeUnit.MILLISECONDS);
	}
	
	public void setHaveControl(boolean havecontrol)
	{
		this.havecontrol = havecontrol;
	}
	
	public void takeSnapshot()
	{
		if (!havecontrol)
			return;
		
		snapshot = SnapshotState.THROWAWAY;
	}
	
	private void sendcontrol()
	{
		if (!havecontrol)
		{
			return;
		}
		
		try
		{	
			URLConnection conn = url.openConnection();
			conn.setDoOutput(true);
			
			DataOutputStream out = new DataOutputStream(conn.getOutputStream());
			out.writeDouble(drive.getYPosition());	//throttle
			out.writeDouble(drive.getXPosition());	//turn
			out.writeDouble(look.getXPosition());	//pan
			out.writeDouble(look.getYPosition());	//tilt
			out.writeBoolean(snapshot != SnapshotState.DISABLED && snapshot != SnapshotState.AFTER);
			out.flush();
			
			out.close();
			
			conn.getInputStream().close();
			
		} catch (IOException e)
		{
			e.printStackTrace();
			//ignore
		}
	}
}
