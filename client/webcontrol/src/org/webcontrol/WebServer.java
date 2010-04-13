package org.webcontrol;

import java.io.File;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;

import org.webcontrol.handler.ControlHandler;
import org.webcontrol.handler.RootHandler;
import org.webcontrol.handler.VideoHandler;

import com.sun.net.httpserver.HttpServer;

public class WebServer {

	public static void main(String[] args) throws IOException
	{
		if (args.length != 1)
		{
			System.err.println("Usage java -jar webserver.jar <port>");
			System.exit(-1);
		}
		
		int port = Integer.parseInt(args[0]);
		
		
		Runnable feed = new FeedClient();
		Thread t = new Thread(feed);
		t.setDaemon(true);
		t.start();
		
		HttpServer server = HttpServer.create(new InetSocketAddress(port), 3);
		server.createContext("/", new RootHandler(new File("www")));
		server.createContext("/video", new VideoHandler());
		server.createContext("/control", new ControlHandler());
		server.setExecutor(Executors.newCachedThreadPool());
		server.start();
		
	}
}
