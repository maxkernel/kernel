package org.webcontrol.handler;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

import org.webcontrol.HttpCodes;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;

public class RootHandler implements HttpHandler
{
	private static final int BUFSIZE = 512;
	private static final String ROOTFILE = "index.html";
	private static final Map<String, String> TYPE = new HashMap<String, String>();
	static
	{
		TYPE.put("html", "text/html");
		TYPE.put("png", "image/png");
		TYPE.put("gif", "image/gif");
		TYPE.put("jpg", "image/jpeg");
	}
	
	private File root;
	
	public RootHandler(File rootdir)
	{
		root = rootdir;
	}
	
	@Override
	public void handle(HttpExchange ex) throws IOException {
		if (!ex.getRequestMethod().equalsIgnoreCase("GET"))
			return;
		
		File fp = new File(root, ex.getRequestURI().toString());
		if (fp.isDirectory())
		{
			fp = new File(fp, ROOTFILE);
		}
		
		if (fp.exists())
		{
			ex.sendResponseHeaders(HttpCodes.HTTP_OKAY, 0);
			
			FileInputStream fin = new FileInputStream(fp);
			OutputStream out = ex.getResponseBody();
			
			byte[] buf = new byte[BUFSIZE];
			int len = 0;
			
			do
			{
				len = fin.read(buf);
				out.write(buf, 0, len);
			} while (len > 0);
			
			/*
			Headers request = ex.getRequestHeaders();
			for (String key : request.keySet())
			{
				System.out.println(key + " = " + request.get(key));
			}
			*/
			
			//Headers response = ex.getResponseHeaders();
			//response.set("Content-Type", "text/plain");
			
			fin.close();
		}
		else
		{
			ex.sendResponseHeaders(HttpCodes.HTTP_NOTFOUND, 0);
		}
		
		ex.close();
	}

}
