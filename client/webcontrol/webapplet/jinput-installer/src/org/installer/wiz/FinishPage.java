package org.installer.wiz;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Font;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;

public class FinishPage extends WizardPagePart
{
	private static final long serialVersionUID = 0L;
	
	public final String CONTENT = "Blah";
	
	public FinishPage()
	{
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
		
		setLayout(new BorderLayout());
		add(scroll, BorderLayout.CENTER);
	}
	
	@Override public String getName() { return "Finish"; }
	@Override public String getDescription() { return "Thank you for installing the Java joystick interface."; }
	@Override public boolean isSkippable() { return false; }
	
	@Override public void setVisible(Map<String, Object> properties, boolean isvisible) {}
	@Override public boolean validate(Map<String, Object> properties) { return true; }
}
