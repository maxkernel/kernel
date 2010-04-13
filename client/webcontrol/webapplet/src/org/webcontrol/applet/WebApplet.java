package org.webcontrol.applet;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.LayoutManager;
import java.io.IOException;
import java.net.URL;

import javax.swing.JApplet;

public class WebApplet extends JApplet implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	private VideoPanel videopanel;
	public WebApplet() throws Exception
	{
		videopanel = new VideoPanel();
		
		setLayout(this);
		add(videopanel);
	}
	
	@Override
	public void start()
	{
		try
		{
			URL base = getDocumentBase();
			videopanel.connect(new URL(base.getProtocol()+"://"+base.getHost()+":"+base.getPort()+"/video"));
			
		} catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	@Override
	public void layoutContainer(Container parent)
	{
		videopanel.setBounds(0, 0, 640, 480);
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
