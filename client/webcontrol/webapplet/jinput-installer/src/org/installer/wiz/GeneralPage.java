package org.installer.wiz;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Font;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;

public class GeneralPage extends WizardPagePart
{
	private static final long serialVersionUID = 0L;
	
	public final String CONTENT = "Java joystick interface software\n\n" +
			"This installer is intended to install a Java joystick interface for use by Java Applets " +
			"and other Java software. The joystick software is provided by JInput (https://jinput.dev.java.net) and is released " +
			"under BSD License Agreement (http://www.opensource.org/licenses/bsd-license.php).\n\n" +
			"For more information, please visit http://www.maxkernel.com";
	
	public GeneralPage()
	{
		setLayout(new BorderLayout());
		
		JTextArea text = new JTextArea(CONTENT);
		text.setFont(new Font("sans-serif", Font.PLAIN, 12));
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
	@Override public String getDescription() { return "Please read over the following information."; }
	@Override public boolean isSkippable() { return false; }
	
	@Override public void setVisible(Map<String, Object> properties, boolean isvisible) {}
	@Override public boolean validate(Map<String, Object> properties) { return true; }
}
