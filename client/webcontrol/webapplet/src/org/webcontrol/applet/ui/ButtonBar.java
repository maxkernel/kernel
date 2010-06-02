package org.webcontrol.applet.ui;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;

import javax.swing.JPanel;

import org.webcontrol.applet.listener.FullscreenListener;
import org.webcontrol.applet.ui.widget.MAbstractButton;
import org.webcontrol.applet.ui.widget.MButton;
import org.webcontrol.applet.ui.widget.MToggleButton;
import org.webcontrol.applet.ui.widget.event.ButtonListener;

public class ButtonBar extends JPanel implements LayoutManager
{
	private FullscreenListener fullscreenlistener;
	
	private MToggleButton fullscreen;
	
	public ButtonBar(FullscreenListener fsl)
	{
		fullscreenlistener = fsl;
		
		setOpaque(false);
		
		fullscreen = new MToggleButton("Fullscreen");
		fullscreen.addButtonListener(new ButtonListener() {
			@Override
			public void buttonPressed(MAbstractButton button) {
				fullscreenlistener.fullscreenRequest(fullscreen.isSelected());
			}
		});
		
		setLayout(this);
		add(fullscreen);
	}

	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		fullscreen.setBounds(w-100, y, 100, h);
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return new Dimension(parent.getWidth(), 30); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
