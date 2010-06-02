package org.webcontrol.applet.client;

import java.net.InetAddress;

import org.webcontrol.applet.listener.ActivationListener;

public class ActivationClient
{
	private ActivationListener listener;
	
	public ActivationClient(ActivationListener al)
	{
		listener = al;
	}
	
	public void connect(final InetAddress addr, final int port)
	{
		Thread t = new Thread(new Runnable() {
			@Override public void run() { mainloop(addr, port); }
		}, "ActivationClient Thread");
		
		t.setDaemon(true);
		t.start();
	}
	
	private void mainloop(InetAddress addr, int port)
	{
		
	}
}
