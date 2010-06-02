package org.installer;

import java.io.IOException;
import java.io.InputStream;

public class FileCache
{
	private static final String PATH = "/org/installer/files/";
	
	public static InputStream fromCache(String name) throws IOException
	{
		return JInputInstaller.class.getResourceAsStream(PATH+name);
	}
}
