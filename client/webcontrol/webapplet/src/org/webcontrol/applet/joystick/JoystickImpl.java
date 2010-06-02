package org.webcontrol.applet.joystick;

import net.java.games.input.Component;
import net.java.games.input.Controller;
import net.java.games.input.ControllerEnvironment;

public class JoystickImpl extends Joystick
{
	private Controller stick;
	
	public JoystickImpl()
	{
		stick = null;
		
		for (Controller c : ControllerEnvironment.getDefaultEnvironment().getControllers())
		{
			if (c.getType().equals(Controller.Type.STICK))
			{
				stick = c;
				break;
			}
		}
	}

	@Override
	public boolean isConnected() {
		return stick != null;
	}
	
	@Override
	public boolean poll() {
		return stick.poll();
	}
	
	private double getAxis(Component.Identifier axis)
	{
		if (!isConnected())
			return 0;
		
		Component c = stick.getComponent(axis);
		return c == null ? 0 : c.getPollData();
	}
	
	private boolean getButton(Component.Identifier butn)
	{
		if (!isConnected())
			return false;
		
		Component c = stick.getComponent(butn);
		return c == null ? false : c.getPollData() > 0;
	}

	@Override public double getR() { return getAxis(Component.Identifier.Axis.RZ); }
	@Override public double getX() { return getAxis(Component.Identifier.Axis.X); }
	@Override public double getY() { return getAxis(Component.Identifier.Axis.Y); }
	@Override public double getPOV() { return getAxis(Component.Identifier.Axis.POV); }
	@Override public boolean getTrigger() { return getButton(Component.Identifier.Button.TRIGGER); }
}
