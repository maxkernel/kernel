package org.webcontrol.applet;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.GraphicsDevice;
import java.awt.GraphicsEnvironment;

import javax.swing.BorderFactory;
import javax.swing.JApplet;
import javax.swing.JPanel;
import javax.swing.JWindow;

import org.webcontrol.applet.listener.FullscreenListener;
import org.webcontrol.applet.ui.ButtonBar;
import org.webcontrol.applet.ui.ContentPanel;

public class WebApplet2 extends JApplet implements FullscreenListener
{
	private static final long serialVersionUID = 1L;
	
	private JWindow window;
	private JPanel content;
	
	public WebApplet2()
	{
		
		window = new JWindow();
		window.setLayout(new BorderLayout());
		
		content = new JPanel();
		content.setOpaque(true);
		content.setBackground(Color.BLACK);
		content.setBorder(BorderFactory.createLineBorder(Color.DARK_GRAY));

		content.setLayout(new BorderLayout());
		content.add(new ButtonBar(this), BorderLayout.NORTH);
		content.add(new ContentPanel(), BorderLayout.CENTER);
		
		setLayout(new BorderLayout());
		add(content, BorderLayout.CENTER);
	}

	@Override
	public void fullscreenRequest(boolean dofullscreen) {
		GraphicsEnvironment ge = GraphicsEnvironment.getLocalGraphicsEnvironment();
		GraphicsDevice gs = ge.getDefaultScreenDevice();
		
		if (dofullscreen)
		{
			remove(content);
			window.add(content, BorderLayout.CENTER);
			gs.setFullScreenWindow(window);
		}
		else
		{
			gs.setFullScreenWindow(null);
			window.remove(content);
			add(content, BorderLayout.CENTER);
		}
		
		validate();
	}
}
