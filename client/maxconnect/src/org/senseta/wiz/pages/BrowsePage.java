package org.senseta.wiz.pages;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.SwingUtilities;

import org.senseta.MaxRobot;
import org.senseta.wiz.ui.RobotBrowser;

public class BrowsePage extends WizardPagePart
{
	private static final long serialVersionUID = 0L;
	
	private RobotBrowser browser;
	private boolean skippable;
	
	public BrowsePage(boolean isskippable)
	{
		skippable = isskippable;
		browser = new RobotBrowser();
		browser.addActionListener(new ActionListener() {
			@Override public void actionPerformed(ActionEvent e) { fireActionEvent(e); }
		});
		
		setLayout(new BorderLayout());
		add(browser, BorderLayout.CENTER);
	}
	
	@Override
	public void setVisible(Map<String, Object> properties, boolean isvisible)
	{
		if (isvisible)
		{
			if (properties.containsKey("robot"))
			{
				MaxRobot robot = (MaxRobot)properties.get("robot");
				
				browser.broadcastNow();
				browser.robotDetected(robot);
				browser.setSelected(robot);
			}
			
			browser.startBroadcastDaemon();
			
			SwingUtilities.invokeLater(new Runnable() {
				@Override
				public void run()
				{
					browser.requestFocus();
				}
			});
		}
		else
		{
			browser.stopBroadcastDaemon();
		}
	}
	
	@Override public String getName() { return "Select Rover"; }
	@Override public String getDescription() { return "Select the target rover, " +
			"or use the controls below to add one."; }
	@Override public void skipped(Map<String, Object> properties) { properties.remove("robot"); }
	
	@Override
	public boolean validate(Map<String, Object> properties)
	{
		MaxRobot r = browser.getSelected();
		if (r != null)
		{
			properties.put("robot", r);
			browser.setBorder(BorderFactory.createEmptyBorder());
			return true;
		}
		else
		{
			properties.remove("robot");
			browser.setBorder(BorderFactory.createLineBorder(Color.RED));
			return false;
		}
	}

	@Override
	public boolean isSkippable() {
		return skippable;
	}
}
