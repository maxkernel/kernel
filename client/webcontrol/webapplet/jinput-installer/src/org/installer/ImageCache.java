package org.installer;

import java.awt.Graphics;
import java.awt.Image;
import java.awt.image.BufferedImage;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;

public class ImageCache
{
	private static final String PATH = "/org/installer/img/";
	private static final String SUFFIX = ".png";
	
	//singleton
	private ImageCache() {}
	
	public static BufferedImage fromCache(String name)
	{
		try
		{
			return ImageIO.read(ImageCache.class.getResourceAsStream(PATH+name+SUFFIX));
		}
		catch (Exception e)
		{
			return blank();
		}
	}
	
	public static BufferedImage scale(Image img, int maxwidth, int maxheight)
	{
		
		BufferedImage buf = toBufferedImage(img);
		buf = toBufferedImage(buf.getScaledInstance(maxwidth, (int)((double)maxwidth*((double)buf.getHeight()/(double)buf.getWidth())), BufferedImage.SCALE_SMOOTH));
		if (buf.getHeight() > maxheight)
		{
			buf = toBufferedImage(buf.getScaledInstance((int)((double)maxheight*((double)buf.getWidth()/(double)buf.getHeight())), maxheight, BufferedImage.SCALE_SMOOTH));
		}
		
		return buf;
	}
	
	public static BufferedImage blank()
	{
		return new BufferedImage(1, 1, BufferedImage.TYPE_INT_ARGB);
	}
	
	private static BufferedImage toBufferedImage(Image img)
	{
		img = new ImageIcon(img).getImage();
		BufferedImage b = new BufferedImage(img.getWidth(null), img.getHeight(null), BufferedImage.TYPE_INT_ARGB);
		Graphics g = b.getGraphics();
		g.drawImage(img, 0, 0, null);
		g.dispose();
		
		return b;
	}
}
