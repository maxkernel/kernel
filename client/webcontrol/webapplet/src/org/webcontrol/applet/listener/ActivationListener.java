package org.webcontrol.applet.listener;

public interface ActivationListener
{
	public void activationError(String error);
	public void serverStatus(boolean status);
	public void robotStatus(boolean status);
}
