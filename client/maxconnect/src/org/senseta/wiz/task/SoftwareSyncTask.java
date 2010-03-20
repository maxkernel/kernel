package org.senseta.wiz.task;

import java.io.File;
import java.io.IOException;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;
import org.senseta.SoftwareCache;
import org.senseta.client.SoftwareBundleClient;
import org.senseta.wiz.Prompt;
import org.senseta.wiz.Prompt.Button;

public class SoftwareSyncTask extends Task<SoftwareBundle>
{
	
	private MaxRobot robot;
	
	public SoftwareSyncTask(MaxRobot robot, TaskListener<SoftwareBundle> listener)
	{
		super(listener);
		this.robot = robot;
	}

	@Override
	public SoftwareBundle call() throws Exception
	{
		SoftwareBundleClient client = null;
		
		appendln("Connecting to rover...");
		try {
			client = new SoftwareBundleClient(robot);
		}
		catch (IOException e)
		{
			throw new Exception("Error while connecting to rover.\nReason: "+e.getMessage());
		}
		
		appendln("Checking software versions...");
		try {
			SoftwareBundle remote = client.getRunningBundle();
			SoftwareBundle local = SoftwareCache.getBundle(remote.getName(), remote.getVersion());
			
			if (local == null)
			{
				if (show(Prompt.newConfirm("The software does not appear to be on your computer.\nShall I download it?")) != Button.YES)
				{
					throw new Exception("Operation aborted.");
				}
				
				local = download(client);
			}
			else if (!remote.equals(local))
			{
				if (show(Prompt.newConfirm("The software version on your computer does not match the version in use.\nShall I overwrite local version?")) != Button.YES)
				{
					throw new Exception("Operation aborted.");
				}
				
				local = download(client);
			}
			
			return local;
		}
		catch (Exception e)
		{
			throw new Exception("Error while syncing software.\nReason: "+e.getMessage(), e);
		}
	}
	
	private SoftwareBundle download(SoftwareBundleClient client) throws Exception
	{
		appendln("Downloading software...");
		File bundlefile = client.downloadBundle();
		
		appendln("Verifying integrity and caching...");
		SoftwareBundle bundle = SoftwareCache.add(bundlefile);
		
		return bundle;
	}
}
