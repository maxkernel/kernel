package org.webcontrol.applet.ui.widget.event;

import java.util.EventListener;

import org.webcontrol.applet.ui.widget.MAbstractButton;

public interface ButtonListener extends EventListener
{
	public void buttonPressed(MAbstractButton button);
}
