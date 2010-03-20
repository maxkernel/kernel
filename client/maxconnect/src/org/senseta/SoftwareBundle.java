package org.senseta;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.Serializable;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.senseta.wiz.uipart.UI_SoftwareBundle;

public class SoftwareBundle implements Comparable<SoftwareBundle>, Serializable
{
	private static final long serialVersionUID = 0L;
	
	public static enum Type { APPLICATION, UPGRADE };
	public static enum OS { ALL(0), WINDOWS(1), LINUX(2), MAC(3);
		private int num;
		private OS(int n) { num = n; }
		public int getValue() { return num; }
	};
	
	private static final Pattern CONFIG_REGEX = Pattern.compile("^([A-Z_]+)=\"?(.*?)\"?$");
	private static final Pattern SERVER_REGEX = Pattern.compile("^([a-zA-Z0-9_]+)=(.*)$");
	private static final Pattern PARAM_REGEX = Pattern.compile("\\{([a-zA-Z0-9_]+):([^\\}]*)\\}");
	
	private Type type = null;
	private double version = 0.0;
	
	private Map<String, String> properties;
	private String cmd[];
	
	private SoftwareBundle(Map<String, String> map) throws IllegalArgumentException
	{
		if (!map.containsKey("name") || !map.containsKey("version") || !map.containsKey("bid") || !map.containsKey("type"))
		{
			System.out.println(map);
			throw new IllegalArgumentException("Software bundle must have defined name, type, version, and bid");
		}
		
		this.properties = Collections.unmodifiableMap(map);
		
		try {
			type = Type.valueOf(map.get("type").toUpperCase());
		} catch (IllegalArgumentException e) {
			throw new IllegalArgumentException("Invalid type: "+map.get("type"), e);
		}
		
		try {
			version = Double.parseDouble(map.get("version"));
		} catch (NumberFormatException e) {
			throw new IllegalArgumentException("Invalid version (must be number): "+map.get("version"), e);
		}
		
		cmd = new String[OS.values().length];
		cmd[OS.ALL.getValue()]		= map.get("ground");
		cmd[OS.WINDOWS.getValue()]	= map.get("ground_windows");
		cmd[OS.LINUX.getValue()]	= map.get("ground_linux");
		cmd[OS.MAC.getValue()]		= map.get("ground_mac");
	}
	
	public UI_SoftwareBundle getUIPart()
	{
		return new UI_SoftwareBundle(this);
	}
	
	public static SoftwareBundle fromConfigFile(BufferedReader reader) throws IOException, IllegalArgumentException
	{
		Map<String, String> map = new HashMap<String, String>();
		
		String line = null;
		while ((line = reader.readLine()) != null)
		{	
			Matcher m = CONFIG_REGEX.matcher(line.trim());
			if (!m.matches())
				continue;
			
			String name = m.group(1);
			String value = m.group(2);
			
			map.put(name.toLowerCase(), value);
		}
		
		return new SoftwareBundle(map);
	}
	
	public static SoftwareBundle fromServerStream(InputStream in) throws IOException, IllegalArgumentException
	{
		Map<String, String> map = new HashMap<String, String>();
		map.put("type", Type.APPLICATION.toString());
		
		int read;
		StringBuffer buf = new StringBuffer();
		do
		{
			read = in.read();
			if (read > 0)
				buf.append((char)read);
		}
		while (read > 0);
		
		String[] entries = buf.toString().split("\n");
		for (String entry : entries)
		{
			Matcher m = SERVER_REGEX.matcher(entry);
			
			if (!m.matches())
			{
				throw new IOException("Unknown/Invalid response from server: "+entry);
			}
			
			String name = m.group(1);
			String value = m.group(2);
			
			map.put(name.toLowerCase(), value);
		}
		
		//backwards compatibility
		if (!map.containsKey("bid") && map.containsKey("id"))
		{
			map.put("bid", map.get("id"));
		}
		
		return new SoftwareBundle(map);
	}
	
	
	public Type getType() { return type; }
	public double getVersion() { return version; }
	public String getName() { return properties.get("name"); }
	public String getIconPath() { return properties.get("icon"); }
	public String getBID() { return properties.get("bid"); }
	public String getDescription() { return properties.get("description"); }
	public String getAuthor() { return properties.get("author"); }
	public String getProvider() { return properties.get("provider"); }
	
	public ProcessBuilder execute(MaxRobot robot, File dir)
	{
		OS os = getOS();
		String cmdstr = cmd[os.getValue()];
		if (cmdstr == null)
		{
			cmdstr = cmd[OS.ALL.getValue()];
		}
		
		if (cmdstr == null)
		{
			throw new RuntimeException("Could not resolve ground station execution command");
		}
		
		StringBuffer buf = new StringBuffer(cmdstr);
		while (true)
		{
			Matcher m = PARAM_REGEX.matcher(buf.toString());
			if (m.find())
			{
				String val = robot == null? null : robot.get(m.group(1));
				if (val == null)
					val = m.group(2);
				
				buf.replace(m.start(), m.end(), val);
				System.out.println(val);
			}
			else
			{
				break;
			}
		}
		
		//System.out.println(buf.toString());
		
		String[] cmdarr = buf.toString().split(" ");
		ProcessBuilder pb = new ProcessBuilder(cmdarr);
		pb.redirectErrorStream(true);
		pb.directory(dir);
		
		return pb;
	}
	
	@Override
	public int compareTo(SoftwareBundle o)
	{
		int cmp;
		if ((cmp = getName().compareTo(o.getName())) == 0)
			return Double.compare(version, o.version);
		
		return cmp;
	}
	
	@Override
	public int hashCode()
	{
		return getBID().hashCode();
	}
	
	@Override
	public boolean equals(Object o)
	{
		if (!(o instanceof SoftwareBundle) || o == null)
			return false;
		
		SoftwareBundle b = (SoftwareBundle)o;
		
		return compareTo(b) == 0 && getBID().equals(b.getBID());
	}
	
	private static OS getOS()
	{
		if (System.getProperty("os.name").toLowerCase().indexOf("windows") > -1)
			return OS.WINDOWS;
		else if (System.getProperty("os.name").toLowerCase().indexOf("linux") > -1)
			return OS.LINUX;
		else if (System.getProperty("os.name").toLowerCase().indexOf("mac") > -1)
			return OS.MAC;
		
		return OS.ALL;
	}
}
