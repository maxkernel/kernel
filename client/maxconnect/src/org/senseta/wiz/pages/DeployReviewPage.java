package org.senseta.wiz.pages;

import java.awt.BorderLayout;
import java.awt.Color;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;

public class DeployReviewPage extends WizardPagePart
{
	private static final long serialVersionUID = 0L;
	
	private JTextArea text;
	
	public DeployReviewPage()
	{
		text = new JTextArea();
		text.setBackground(new Color(0xEBEBEB));
		text.setEditable(false);
		text.setTabSize(2);
		text.setLineWrap(true);
		text.setWrapStyleWord(true);
		
		setLayout(new BorderLayout());
		JScrollPane scroll = new JScrollPane(text, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		text.setBorder(BorderFactory.createEmptyBorder(3, 5, 3, 3));
		scroll.setBorder(BorderFactory.createEmptyBorder());
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		add(scroll, BorderLayout.CENTER);
	}
	
	@Override
	public void setVisible(Map<String, Object> map, boolean isvisible)
	{
		if (!isvisible)
			return;
		
		MaxRobot robot = (MaxRobot)map.get("robot");
		SoftwareBundle bundle = (SoftwareBundle)map.get("software");
		
		String txt = "======= Review Configuration =======\n\n" +
				"** Software **\n" +
				"Bundle: "+bundle.getName()+"\n" +
				"Type: "+bundle.getType().toString().toLowerCase()+"\n" +
				"Bundle ID: "+bundle.getBID()+"\n" +
				"Version: "+Double.toString(bundle.getVersion())+"\n\n" +
				"** Rover **\n" +
				"Name: "+robot.getName()+"\n" +
				"Rover ID: "+robot.getID()+"\n" +
				"Rover IP: "+robot.getIP()+"\n" +
				"MaxKernel: "+robot.getVersion()+"\n" +
				"Provider: "+robot.getProvider()+"\n\n" +
				"** User Credentials **\n" +
				"Username: "+map.get("username")+"\n" +
				"Password: (given)";
		
		text.setText(txt);
		text.setCaretPosition(0);
	}
	
	@Override public String getName() { return "Review"; }
	@Override public String getDescription() { return "Please review the installation details. " +
			"This is the last chance to go back and modify them before the settings become permanent."; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public boolean validate(Map<String, Object> properties)
	{
		return true;
	}

}
