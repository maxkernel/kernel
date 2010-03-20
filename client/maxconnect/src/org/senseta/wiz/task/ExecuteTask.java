package org.senseta.wiz.task;

import java.io.File;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;
import org.senseta.SoftwareCache;

public class ExecuteTask extends Task<ProcessBuilder>
{
	private MaxRobot robot;
	private SoftwareBundle bundle;
	
	public ExecuteTask(MaxRobot robot, SoftwareBundle bundle, TaskListener<ProcessBuilder> listener)
	{
		super(listener);
		this.robot = robot;
		this.bundle = bundle;
	}

	@Override
	public ProcessBuilder call() throws Exception
	{
		try {
			appendln("Unpacking software...");
			File dir = SoftwareCache.extract(bundle);
			
			appendln("Starting up...");
			ProcessBuilder pb = bundle.execute(robot, dir);
			
			return pb;
		}
		catch (Exception e)
		{
			throw new Exception("Could not start up software.\nReason: "+e.getMessage(), e);
		}
	}
}
