package org.installer;

import java.io.IOException;
import java.io.InputStream;

public class JInputInstaller
{
	private static final String ARCHIVE_PREFIX = "/org/installer/files/";
	public static enum OS {
		UNKNOWN(null, null),
		WINDOWS(
				new String[]{"C:\\Program Files\\Java"},
				new String[]{"jinput.jar", "jinput-dx8.dll", "jinput-raw.dll", "jinput-wintab.dll"}),
		LINUX(
				new String[]{"/usr/lib/jvm"},
				new String[]{"jinput.jar", "libjinput-linux.so"}),
		MAC(
				new String[]{},
				new String[]{"jinput.jar", "libjinput-osx.jnilib"});
		
		public String[] search;
		public String[] files;
		private OS(String[] search, String[] files)
		{
			this.search = search;
			this.files = files;
		}
	};
	
	private static OS getOS()
	{
		if (System.getProperty("os.name").toLowerCase().indexOf("windows") > -1)
			return OS.WINDOWS;
		else if (System.getProperty("os.name").toLowerCase().indexOf("linux") > -1)
			return OS.LINUX;
		else if (System.getProperty("os.name").toLowerCase().indexOf("mac") > -1)
			return OS.MAC;
		
		return OS.UNKNOWN;
	}
	
	private static InputStream getFile(String name) throws IOException
	{
		return JInputInstaller.class.getResourceAsStream(ARCHIVE_PREFIX+name);
	}
	
	public static void main(String[] args)
	{
		OS os = getOS();
		
	}
}
