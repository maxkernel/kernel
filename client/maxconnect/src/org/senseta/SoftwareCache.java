package org.senseta;

import java.awt.image.BufferedImage;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

import javax.imageio.ImageIO;

import org.senseta.wiz.ImageCache;


public class SoftwareCache
{
	//static class
	private SoftwareCache() {}
	
	private static final String NAME_REGEX = "^[A-Za-z_]+\\-\\d+(?:\\.\\d+)\\.stuff$";
	private static final File CACHE_FOLDER;
	private static final Map<SoftwareBundle, File> DB;
	
	static {
		CACHE_FOLDER = new File(System.getProperty("user.home"), ".deploycache");
		DB = Collections.synchronizedMap(new HashMap<SoftwareBundle, File>());
		
		CACHE_FOLDER.mkdirs();
		for (File f : CACHE_FOLDER.listFiles())
		{
			try {
				if (f.isFile() && f.canRead() && Pattern.matches(NAME_REGEX, f.getName()))
				{
					SoftwareBundle b = parseBundle(f);
					DB.put(b, f);
				}
			} catch (Exception e) {} //ignore
		}
	}
	
	private static SoftwareBundle parseBundle(File fp) throws IOException, IllegalArgumentException
	{
		ZipFile zip = new ZipFile(fp);
		ZipEntry config = zip.getEntry("config");
		if (config == null)
		{
			throw new IOException("Config file not found in bundle");
		}
		
		BufferedReader r = new BufferedReader(new InputStreamReader(zip.getInputStream(config)));
		SoftwareBundle b = SoftwareBundle.fromConfigFile(r);
		r.close();
		zip.close();
		
		return b;
	}
	
	public static List<SoftwareBundle> getCache()
	{
		return Collections.unmodifiableList(new ArrayList<SoftwareBundle>(DB.keySet()));
	}
	
	public static BufferedImage getIcon(SoftwareBundle bundle)
	{
		String iconpath = bundle.getIconPath();
		File fp = DB.get(bundle);
		
		if (fp == null || iconpath == null)
		{
			return ImageCache.blank();
		}
		
		ZipFile zip = null;
		BufferedImage icon = null;
		
		try
		{
			zip = new ZipFile(fp);
			ZipEntry entry = zip.getEntry(iconpath);
			if (entry == null)
			{
				return ImageCache.blank();
			}
			
			icon = ImageIO.read(zip.getInputStream(entry));
		}
		catch (IOException e)
		{
			return ImageCache.blank();
		}
		finally
		{
			try
			{
				if (zip != null) zip.close();
			}
			catch (IOException e) {} //ignore
		}
		
		return icon;
	}
	
	public static SoftwareBundle getBundle(String name, double version)
	{
		for (SoftwareBundle b : DB.keySet())
		{
			if (b.getName().equals(name) && b.getVersion() == version)
			{
				return b;
			}
		}
		
		return null;
	}
	
	public static File getFile(SoftwareBundle bundle)
	{
		return DB.get(bundle);
	}
	
	public static boolean exists(File fp)
	{
		try {
			SoftwareBundle b1 = parseBundle(fp);
			SoftwareBundle b2 = getBundle(b1.getName(), b1.getVersion());
			return b2 != null;
		}
		catch (Exception e)
		{
			return false;
		}
	}
	
	public static SoftwareBundle add(File fp) throws IOException, IllegalArgumentException
	{
		SoftwareBundle b = parseBundle(fp);
		
		File target = new File(CACHE_FOLDER, String.format("%s-%s.stuff", b.getName(), Double.toString(b.getVersion())));
		target.delete(); //delete file if exists
		
		//copy file to cache
		FileChannel fin = new FileInputStream(fp).getChannel();
		FileChannel fout = new FileOutputStream(target).getChannel();
		
		long size = fin.size();
		MappedByteBuffer buf = fin.map(MapMode.READ_ONLY, 0, size);
		fout.write(buf);
		fin.close();
		fout.close();
		
		DB.put(b, target);
		return b;
	}
	
	public static void remove(SoftwareBundle bundle)
	{
		if (!DB.containsKey(bundle))
		{
			return;
		}
		
		File fp = DB.remove(bundle);
		fp.delete();
	}
	
	public static File extract(SoftwareBundle b) throws IOException
	{
		File archive = DB.get(b);
		if (archive == null)
		{
			throw new IOException("Software Bundle "+b.getName()+" is not in the cache");
		}
		
		File dir = File.createTempFile("bundle.extract", ".stuff");
		dir.delete();
		dir.mkdir();
		dir.deleteOnExit();
		
		ZipInputStream zis = new ZipInputStream(new FileInputStream(archive));
		ZipEntry entry;
		
		byte[] buf = new byte[1024];
		int read;
		
		while ((entry = zis.getNextEntry()) != null)
		{
			File fp = new File(dir, entry.getName());
			if (entry.isDirectory())
			{
				fp.mkdir();
			}
			else
			{
				FileOutputStream out = new FileOutputStream(fp);
				do
				{
					read = zis.read(buf);
					if (read > 0)
						out.write(buf, 0, read);
				}
				while (read > 0);
				out.close();
				
				fp.setExecutable(true, true);
			}
			
			fp.deleteOnExit();
			zis.closeEntry();
		}
		
		zis.close();
		
		return dir;
	}
	
	/*
	public static SoftwareBundle addToCache(File fp) throws IOException
	{
		File out = new File(CACHE_FOLDER, fp.getName());
		try {
			if (out.exists())
				throw new IOException("Bundle "+fp.getName()+" already exists in cache");
			
			//copy file to cache
			FileChannel fin = new FileInputStream(fp).getChannel();
			FileChannel fout = new FileOutputStream(out).getChannel();
			
			long size = fin.size();
			MappedByteBuffer buf = fin.map(MapMode.READ_ONLY, 0, size);
			fout.write(buf);
			fin.close();
			fout.close();
			
			return new SoftwareBundle(out);
			
		} catch (IOException e) {
			out.delete();
			throw e; //rethrow
		}
	}
	
	public static void removeFromCache(SoftwareBundle b)
	{
		b.getFile().delete();
	}
	
	public static void removeFromCache(String name)
	{
		new File(CACHE_FOLDER, name).delete();
	}
	
	public static boolean existsInCache(String name)
	{
		for (File f : CACHE_FOLDER.listFiles())
		{
			if (f.getName().equals(name))
				return true;
		}
		
		return false;
	}
	
	public static SoftwareBundle[] getCache()
	{
		List<SoftwareBundle> l = new ArrayList<SoftwareBundle>();
		
		for (File f : CACHE_FOLDER.listFiles())
		{
			if (f.isFile() && f.getName().endsWith(".stuff"))
			{
				try {
					l.add(new SoftwareBundle(f));
				} catch (IOException e) {} //ignore
			}
		}
		
		return l.toArray(new SoftwareBundle[l.size()]);
	}
	
	public static SoftwareBundle getFromCache(String name) throws IOException
	{
		File f = new File(CACHE_FOLDER, name);
		if (!f.exists())
			return null;
		
		return new SoftwareBundle(f);
	}
	
	public static File getTempDir()
	{
		Random r = new Random(System.currentTimeMillis());
		File d = null;
		
		do
		{
			d = new File(CACHE_FOLDER, "cache."+r.nextInt(10000));
		}
		while (d.exists());
		
		d.mkdir();
		d.deleteOnExit();
		
		return d;
	}
	*/
}
