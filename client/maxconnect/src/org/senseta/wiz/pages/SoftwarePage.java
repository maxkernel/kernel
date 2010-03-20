package org.senseta.wiz.pages;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.SwingUtilities;

import org.senseta.SoftwareBundle;
import org.senseta.wiz.ui.SoftwareBrowser;

public class SoftwarePage extends WizardPagePart
{
	private static final long serialVersionUID = 0L;
	
	private SoftwareBrowser browser;
	private boolean skipifknown;
	
	public SoftwarePage(boolean skipifknown /* skip page if software can be determined via autodetect */)
	{
		this.skipifknown = skipifknown;
		browser = new SoftwareBrowser();
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
			if (skipifknown && properties.containsKey("robot"))
			{
				fireActionEvent(new ActionEvent(this, 0, ""));
				return;
			}
			
			SwingUtilities.invokeLater(new Runnable() {
				@Override
				public void run()
				{
					browser.requestFocus();
				}
			});
		}
	}
	
	
	@Override public String getName() { return "Choose Software"; }
	@Override public String getDescription() { return "Select the software bundle. If you don't see the software, " +
			"you can browse for the bundle on your local hard drive."; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public boolean validate(Map<String, Object> properties)
	{
		if (skipifknown && properties.containsKey("robot"))
		{
			return true;
		}
		
		SoftwareBundle b = browser.getSelected();
		if (b != null)
		{
			properties.put("software", b);
			browser.setBorder(BorderFactory.createEmptyBorder());
			return true;
		}
		else
		{
			browser.setBorder(BorderFactory.createLineBorder(Color.RED));
			return false;
		}
	}

}
