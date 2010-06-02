package org.installer;

public enum OS
{	
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
	
	public enum Arch { UNKNOWN, x86, x86_64 };
	
	public String[] search;
	public String[] files;
	private OS(String[] search, String[] files)
	{
		this.search = search;
		this.files = files;
	}
	
	public static OS getOS()
	{
		if (System.getProperty("os.name").toLowerCase().contains("windows"))
			return WINDOWS;
		else if (System.getProperty("os.name").toLowerCase().contains("linux"))
			return LINUX;
		else if (System.getProperty("os.name").toLowerCase().contains("mac"))
			return MAC;
		
		return UNKNOWN;
	}
	
	public static Arch getArch()
	{
		if (System.getProperty("os.arch").contains("64"))
			return Arch.x86_64;
		else if (System.getProperty("os.arch").contains("86"))
			return Arch.x86;
		
		return Arch.UNKNOWN;
	}
}
