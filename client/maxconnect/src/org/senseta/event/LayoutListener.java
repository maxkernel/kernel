package org.senseta.event;

import java.util.EventListener;

import javax.swing.JComponent;

public interface LayoutListener extends EventListener
{
	public void layoutTriggered(JComponent component);
}
