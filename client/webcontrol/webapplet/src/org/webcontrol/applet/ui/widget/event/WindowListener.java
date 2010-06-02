package org.webcontrol.applet.ui.widget.event;

import java.util.EventListener;

import org.webcontrol.applet.ui.widget.MWindow;

public interface WindowListener extends EventListener
{
	public void windowSelected(MWindow window);
}
