package org.senseta.client;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketTimeoutException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.swing.event.EventListenerList;

import org.senseta.MaxRobot;
import org.senseta.event.BrowseListener;

public class DiscoveryClient
{
	private static final Logger LOG = Logger.getLogger(DiscoveryClient.class.getName());
	
	private static final int QUERY_PORT = 50091;
	
	private static final String BROADCAST_MESSAGE = "discover";
	private static final int BROADCAST_DELAY = 1500;				//1.5 sec
	private static final int SOCKET_TIMEOUT = 500;					//0.5 sec
	
	private ScheduledExecutorService tasks;
	private List<MaxRobot> detected;
	private EventListenerList listeners;
	
	private ScheduledFuture<?> broadcaster;
	
	public DiscoveryClient()
	{
		detected = Collections.synchronizedList(new ArrayList<MaxRobot>());
		listeners = new EventListenerList();
		
		broadcaster = null;
		
		tasks = Executors.newScheduledThreadPool(1, new ThreadFactory() {
			@Override
			public Thread newThread(Runnable r)
			{
				Thread t = new Thread(r);
				t.setDaemon(true);
				return t;
			}
		});
	}
	
	public MaxRobot detectName(String name)
	{
		ScheduledFuture<MaxRobot> future = tasks.schedule(new DetectNameTask(name), 0, TimeUnit.MILLISECONDS);
		MaxRobot robot = null;
		
		try {
			robot = future.get();
		}
		catch (Exception e)
		{
			LOG.log(Level.INFO, "Could not manually detect robot with name: "+name, e);
		}
		
		return robot;
	}
	
	public MaxRobot detectIP(String ip)
	{
		ScheduledFuture<MaxRobot> future = tasks.schedule(new DetectIPTask(ip), 0, TimeUnit.MILLISECONDS);
		MaxRobot robot = null;
		
		try {
			robot = future.get();
		}
		catch (Exception e)
		{
			LOG.log(Level.INFO, "Could not manually detect robot with ip: "+ip, e);
		}
		
		return robot;
	}
	
	public void broadcastNow()
	{
		Future<?> f = tasks.submit(new BroadcastTask());
		try {
			f.get();
		} catch (Exception e) {} //ignore
	}
	
	public void startBroadcastDaemon()
	{
		if (broadcaster != null)
		{
			stopBroadcastDaemon();
		}
		
		broadcaster = tasks.scheduleWithFixedDelay(new BroadcastTask(), 0, BROADCAST_DELAY, TimeUnit.MILLISECONDS);
	}
	
	public void stopBroadcastDaemon()
	{
		if (broadcaster == null)
		{
			return;
		}
		
		broadcaster.cancel(true);
		broadcaster = null;
	}
	
	public List<MaxRobot> getDetectedRobots()
	{
		return Collections.unmodifiableList(detected);
	}
	
	public void clearList()
	{
		detected.clear();
		fireRobotListCleared();
	}
	
	public void addBrowseListener(BrowseListener l)
	{
		listeners.add(BrowseListener.class, l);
	}
	
	public void removeBrowseListener(BrowseListener l)
	{
		listeners.remove(BrowseListener.class, l);
	}
	
	private MaxRobot parseResponse(String data, String ip)
	{
		try {
			MaxRobot robot = MaxRobot.make(data, ip);
			
			if (detected.contains(robot))
			{
				//already detected this one
				return detected.get(detected.indexOf(robot));
			}
			
			detected.add(robot);
			fireRobotDetected(robot);
			
			return robot;
		}
		catch (ParseException e)
		{
			LOG.log(Level.WARNING, "Exception occured while parsing response", e);
			return null;
		}
	}
	
	private void fireRobotDetected(MaxRobot robot)
	{
		Object[] list = listeners.getListenerList();
		for (int i=list.length-2; i>=0; i-=2)
		{
			if (list[i] == BrowseListener.class)
			{
				try {
					((BrowseListener)list[i+1]).robotDetected(robot);
				}
				catch (Throwable t)
				{
					LOG.log(Level.WARNING, "Event listener (BrowseListener) threw an exception", t);
				}
			}
		}
	}
	
	private void fireRobotListCleared()
	{
		Object[] list = listeners.getListenerList();
		for (int i=list.length-2; i>=0; i-=2)
		{
			if (list[i] == BrowseListener.class)
			{
				try {
					((BrowseListener)list[i+1]).robotListCleared();
				}
				catch (Throwable t)
				{
					LOG.log(Level.WARNING, "Event listener (BrowseListener) threw an exception", t);
				}
			}
		}
	}
	
	private class BroadcastTask implements Runnable
	{
		@Override
		public void run()
		{
			try {
				//broadcast
				byte[] buf = BROADCAST_MESSAGE.getBytes();
				DatagramPacket packet = new DatagramPacket(buf, buf.length);
				packet.setPort(QUERY_PORT);
				
				DatagramSocket sock = new DatagramSocket();
				sock.setSoTimeout(SOCKET_TIMEOUT);
				
				for (Enumeration<NetworkInterface> ns = NetworkInterface.getNetworkInterfaces(); ns.hasMoreElements(); )
				{
					NetworkInterface n = ns.nextElement();
					
					if (!n.isUp())
						continue;
					
					for (InterfaceAddress a : n.getInterfaceAddresses())
					{
						InetAddress bc = null;
						if ((bc = a.getBroadcast()) == null)
							continue;
						
						packet.setAddress(bc);
						sock.send(packet);
					}
				}
				
				buf = new byte[500];
				packet.setData(buf);
				
				try {
					while (true)
					{
						sock.receive(packet);
						parseResponse(new String(buf), packet.getAddress().getHostAddress());
					}
				} catch (SocketTimeoutException ex) {
					//ignore, should happen if we haven't received a response from any robot after given timeout
				}
				
				sock.disconnect();
				sock.close();
					
			} catch (IOException e) {
				LOG.log(Level.WARNING, "An exception occured in the broadcast task", e);
			}
		}
	}
	
	private class DetectNameTask implements Callable<MaxRobot>
	{
		private String name;
		
		private DetectNameTask(String name)
		{
			this.name = name;
		}
		
		@Override
		public MaxRobot call()
		{
			try {
				byte[] buf = ("name="+name).getBytes();
				DatagramPacket packet = new DatagramPacket(buf, buf.length);
				packet.setPort(QUERY_PORT);
				DatagramSocket sock = new DatagramSocket();
				for (Enumeration<NetworkInterface> ns = NetworkInterface.getNetworkInterfaces(); ns.hasMoreElements(); )
				{
					NetworkInterface n = ns.nextElement();
					
					if (!n.isUp())
						continue;
					
					for (InterfaceAddress a : n.getInterfaceAddresses())
					{
						InetAddress bc = null;
						if ((bc = a.getBroadcast()) == null)
							continue;
						
						packet.setAddress(bc);
						sock.send(packet);
					}
				}
				
				//get response
				sock.setSoTimeout(SOCKET_TIMEOUT);
				
				buf = new byte[500];
				packet.setData(buf);
				
				MaxRobot robot = null;
				try {
					sock.receive(packet);
					robot = parseResponse(new String(buf), packet.getAddress().getHostAddress());
				} catch (SocketTimeoutException ex) {
					//ignore, should happen if we haven't received a response from any robot after given timeout
				}
				
				sock.disconnect();
				sock.close();
				
				return robot;
					
			} catch (IOException e) {
				LOG.log(Level.WARNING, "An exception occured in the manual name detect task", e);
				return null;
			}
		}
	}
	
	private class DetectIPTask implements Callable<MaxRobot>
	{
		private String ip;
		
		private DetectIPTask(String ip)
		{
			this.ip = ip;
		}
		
		@Override
		public MaxRobot call()
		{
			try {
				//broadcast
				byte[] buf = BROADCAST_MESSAGE.getBytes();
				DatagramPacket packet = new DatagramPacket(buf, buf.length);
				packet.setPort(QUERY_PORT);
				packet.setAddress(InetAddress.getByName(ip));
				
				DatagramSocket sock = new DatagramSocket();
				sock.send(packet);
				
				//get response
				sock.setSoTimeout(SOCKET_TIMEOUT);
				
				buf = new byte[500];
				packet.setData(buf);
				
				MaxRobot robot = null;
				try {
					sock.receive(packet);
					robot = parseResponse(new String(buf), packet.getAddress().getHostAddress());
				} catch (SocketTimeoutException ex) {
					//ignore, should happen if we haven't received a response from any robot after given timeout
				}
				
				sock.disconnect();
				sock.close();
				
				return robot;
					
			} catch (IOException e) {
				LOG.log(Level.WARNING, "An exception occured in the manual ip detect task", e);
				return null;
			}
		}
	}
}
