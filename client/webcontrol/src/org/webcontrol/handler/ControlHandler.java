package org.webcontrol.handler;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.Charset;

import org.webcontrol.HttpCodes;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;

public class ControlHandler implements HttpHandler
{
	private DatagramSocket sock;
	private InetAddress address;
	private int port;
	
	public ControlHandler(InetAddress rover, int port)
	{
		address = rover;
		this.port = port;
		
		try
		{
			sock = new DatagramSocket();
		} catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	@Override
	public void handle(HttpExchange ex) throws IOException {
		//ex.getRequestURI();
		//ex.sendResponseHeaders(HttpCodes.HTTP_OKAY, 0);
		
		DataInputStream in = new DataInputStream(new BufferedInputStream(ex.getRequestBody()));
		try
		{
			//read in the values and properly scale them
			double thro = in.readDouble();
			double turn = in.readDouble() * (-5.0 * Math.PI / 36.0);
			double pan = in.readDouble() * (Math.PI);
			double tilt = in.readDouble() * (Math.PI / 2.0);
			boolean snapshot = in.readBoolean();
			
			//send it away
			DatagramPacket packet = new DatagramPacket(new byte[0], 0);
			packet.setAddress(address);
			packet.setPort(port);
			
			packet.setData(("0"+thro).getBytes(Charset.forName("US-ASCII")));
			sock.send(packet);
			
			packet.setData(("1"+(snapshot? "1" : "0")).getBytes(Charset.forName("US-ASCII")));
			sock.send(packet);
			
			packet.setData(("2"+turn).getBytes(Charset.forName("US-ASCII")));
			sock.send(packet);
			
			packet.setData(("5"+pan).getBytes(Charset.forName("US-ASCII")));
			sock.send(packet);
			
			packet.setData(("6"+tilt).getBytes(Charset.forName("US-ASCII")));
			sock.send(packet);
			
		} catch (IOException e)
		{
			e.printStackTrace();
		}
		
		in.close();
		ex.sendResponseHeaders(HttpCodes.HTTP_OKAY, 0);
		ex.getResponseBody().close();
		
		ex.close();
	}
	
}
