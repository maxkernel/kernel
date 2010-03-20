package org.senseta.wiz.pages;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusAdapter;
import java.awt.event.FocusEvent;
import java.util.Map;

import javax.swing.JLabel;
import javax.swing.JPasswordField;
import javax.swing.JTextField;

import org.senseta.SoftwareBundle;
import org.senseta.SoftwareBundle.Type;

public class CredentialsPage extends WizardPagePart implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	private JLabel title;
	private JLabel ulabel, plabel;
	private JTextField username;
	private JPasswordField password;
	
	public CredentialsPage()
	{
		ulabel = new JLabel("Username:");
		plabel = new JLabel("Password:");
		
		ActionListener act = new ActionListener() {
			@Override public void actionPerformed(ActionEvent e) { fireActionEvent(e); }
		};
		
		title = new JLabel();
		title.setForeground(Color.RED);
		
		username = new JTextField();
		username.addActionListener(act);
		username.addFocusListener(new FocusAdapter() {			
			@Override
			public void focusGained(FocusEvent e)
			{
				username.setSelectionStart(0);
				username.setSelectionEnd(username.getText().length());
			}
		});
		
		password = new JPasswordField();
		password.addActionListener(act);
		password.addFocusListener(new FocusAdapter() {
			@Override
			public void focusGained(FocusEvent e)
			{
				password.setSelectionStart(0);
				password.setSelectionEnd(new String(password.getPassword()).length());
			}
		});
		
		setLayout(this);
		add(title);
		add(ulabel);
		add(plabel);
		add(username);
		add(password);
	}
	
	@Override public void setVisible(Map<String, Object> properties, boolean isvisible)
	{
		SoftwareBundle bundle = (SoftwareBundle)properties.get("software");
		if (bundle.getType() == Type.UPGRADE)
		{
			title.setText("* Admin credentials are required to install upgrades");
		}
		else
		{
			title.setText("");
		}
		
		username.requestFocus();
	}
	
	@Override public String getName() { return "User Credentials"; }
	@Override public String getDescription() { return "Enter the username and password to the rover."; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public boolean validate(Map<String, Object> properties)
	{
		if (username.getText().length() > 0)
		{
			ulabel.setForeground(Color.BLACK);
			properties.put("username", username.getText());
			properties.put("password", new String(password.getPassword()));
			return true;
		}
		else
		{
		
			ulabel.setForeground(Color.RED);
			username.requestFocus();
			return false;
		}
	}

	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		
		title.setBounds(x+10, y+8, w-10, 20);
		
		ulabel.setBounds(x+10, y+35, 100, 20);
		username.setBounds(x+110, y+35, 250, 20);
		
		plabel.setBounds(x+10, y+60, 100, 20);
		password.setBounds(x+110, y+60, 250, 20);
		
	}

	@Override public Dimension minimumLayoutSize(Container parent) {return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
