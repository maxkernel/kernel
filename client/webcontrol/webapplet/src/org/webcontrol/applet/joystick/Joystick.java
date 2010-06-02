package org.webcontrol.applet.joystick;

public abstract class Joystick
{
	
	public abstract boolean isConnected();
	public abstract boolean poll();
	public abstract double getX();
	public abstract double getY();
	public abstract double getR();
	public abstract double getPOV();
	public abstract boolean getTrigger();
	
	
	public static Joystick openJoystick()
	{
		try
		{
			Class.forName("net.java.games.input.ControllerEnvironment");
			return new JoystickImpl();			
		} catch (Exception e)
		{
			return null;
		}
	}
}
