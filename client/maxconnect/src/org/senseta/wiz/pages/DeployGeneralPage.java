package org.senseta.wiz.pages;

import java.awt.BorderLayout;
import java.awt.Color;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;


public class DeployGeneralPage extends WizardPagePart
{
	private static final long serialVersionUID = 0L;
	
	public final String CONTENT = "This installer is intended to deploy a software bundle on a max rover.\n" +
			"This installer is only needed when new software is being deployed or reinstalled. Please refer to user " +
			"documentation for more information.\n\n" +
			"New software bundles are available on the MaxKernel website or through third party sources.\n\n" +
			"This installer is broken down into steps, first you will choose the bundle to install, then you will choose " +
			"the target rover and enter in your credentials. Finally, you will be given a chance to review your changes before " +
			"making them permanent.\n\n" +
			"Before you start, you should complete the following steps:\n" +
			"\t* Have your user credentials handy (username and password)\n" +
			"\t* Obtain the software bundle to install (some versions are provided)\n" +
			"\t* Boot the rover\n\n" +
			"This installer is pretty straight forward. However, as hard as we try to prevent it, errors are bound to happen. If this installer doesn't do " +
			"what it's supposed to do, please don't be silent. Post the output, any errors that you noticed, the software bundle " +
			"name and version and any other relevant information on the MaxKernel troubleshoot forum.\n\n" +
			"Finally, once the software bundle is installed, you can start up the ground station component using the startup program " +
			"that came with this software.";
	
	public DeployGeneralPage()
	{
		setLayout(new BorderLayout());
		
		JTextArea text = new JTextArea(CONTENT);
		text.setBackground(new Color(0xEBEBEB));
		text.setEditable(false);
		text.setLineWrap(true);
		text.setWrapStyleWord(true);
		text.setTabSize(2);
		
		JScrollPane scroll = new JScrollPane(text, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		text.setBorder(BorderFactory.createEmptyBorder(3, 3, 3, 3));
		scroll.setBorder(BorderFactory.createEmptyBorder());
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		add(scroll, BorderLayout.CENTER);
	}
	
	@Override public String getName() { return "General"; }
	@Override public String getDescription() { return "Please read over this information. It is important."; }
	@Override public boolean isSkippable() { return false; }
	
	@Override public void setVisible(Map<String, Object> properties, boolean isvisible) {}
	@Override public boolean validate(Map<String, Object> properties) { return true; }
}
