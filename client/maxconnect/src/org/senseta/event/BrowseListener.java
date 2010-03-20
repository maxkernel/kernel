package org.senseta.event;

import java.util.EventListener;

import org.senseta.MaxRobot;

public interface BrowseListener extends EventListener
{
	public void robotDetected(MaxRobot newrobot);
	public void robotListCleared();
}
