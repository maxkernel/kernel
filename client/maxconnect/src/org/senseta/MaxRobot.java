package org.senseta;

import java.text.ParseException;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.senseta.wiz.uipart.UI_MaxRobot;

public class MaxRobot implements Comparable<MaxRobot>
{
	private static final Pattern ENTRY_PATTERN = Pattern.compile("^([a-zA-z0-9_]+)=(.*)$");
	
	private Map<String, String> properties;
	private long started = 0;
	
	private MaxRobot(Map<String, String> prop) throws IllegalArgumentException
	{
		if (!prop.containsKey("id") || !prop.containsKey("name") || !prop.containsKey("ip"))
		{
			throw new IllegalArgumentException("Map parameter must contain keys id, name, and ip");
		}
		
		properties = prop;
		
		try {
			long started = Long.parseLong(prop.get("started"));
			long now = Long.parseLong(prop.get("now"));
			
			this.started = System.currentTimeMillis()-(now-started)*1000;
		}
		catch (Exception e)
		{
			this.started = 0;
		}
	}
	
	public static MaxRobot make(String data, String ip) throws ParseException
	{
		Map<String, String> e = new HashMap<String, String>();
		e.put("ip", ip);
		
		String[] entries = data.split("\n");
		for (String entry : entries)
		{
			if (entry.charAt(0) == 0)
				continue;
			
			Matcher m = ENTRY_PATTERN.matcher(entry);
			if (!m.matches())
			{
				throw new ParseException("Invalid entry: '"+entry.length()+"'", 0);
			}
			
			e.put(m.group(1), m.group(2));
		}
		
		if (!e.containsKey("id"))
		{
			throw new ParseException("Missing identification tag", 0);
		}
		
		return new MaxRobot(e);
	}
	
	public static MaxRobot makeUnverified(String ip)
	{
		Map<String, String> map = new HashMap<String, String>();
		map.put("id", "unknown");
		map.put("name", ip);
		map.put("ip", ip);
		
		return new MaxRobot(map);
	}
	
	public String get(String key)
	{
		return properties.get(key);
	}
	
	public String getID() { return properties.get("id"); }
	public String getIP() { return properties.get("ip"); }
	public String getName() { return properties.get("name"); }
	
	public String getModel()
	{
		String v = properties.get("model");
		return v==null? "unknown" : v;
	}
	
	public String getVersion()
	{
		String v = properties.get("version");
		return v==null? "0.0" : v;
	}
	
	public String getProvider()
	{
		String v = properties.get("provider");
		return v==null? "" : v;
	}
	
	public long getUptime()
	{
		return started == 0? 0 : System.currentTimeMillis()-started;
	}
	
	public UI_MaxRobot getUIPart()
	{
		return new UI_MaxRobot(this);
	}

	@Override
	public int compareTo(MaxRobot o)
	{
		String thisname = getName();
		String thatname = o.getName();
		
		if (thisname == null || thatname == null)
			return 0;
		
		return thisname.compareTo(thatname);
	}
	
	@Override
	public boolean equals(Object o)
	{
		if (!(o instanceof MaxRobot))
			return false;
		
		MaxRobot or = (MaxRobot)o;
		
		String thisid = getID();
		String thatid = or.getID();
		
		if (thisid == null || thatid == null)
			return false;
		
		return thisid.equals(thatid) && compareTo(or) == 0;
	}
}
