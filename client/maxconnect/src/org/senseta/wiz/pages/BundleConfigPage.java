package org.senseta.wiz.pages;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.io.File;
import java.util.Map;
import java.util.prefs.Preferences;

import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JTextField;

import org.senseta.wiz.uipart.DirectoryChooser;
import org.senseta.wiz.uipart.FileSelectionTree;

public class BundleConfigPage extends WizardPagePart implements LayoutManager
{
	static Preferences prefs = Preferences.userRoot().node("/senseta/BundleWizard");
	
	private JLabel l_projectdir;
	private JTextField t_projectdir;
	private JButton b_browse;
	
	public BundleConfigPage()
	{
		l_projectdir = new JLabel("Project Directory");
		
		t_projectdir = new JTextField();
		t_projectdir.addMouseListener(new MouseAdapter() {
			@Override
			public void mouseClicked(MouseEvent e) {
				if (t_projectdir.getText().length() == 0)
				{
					b_browse.doClick();
				}
			}
		});
		
		b_browse = new JButton("...");
		b_browse.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				DirectoryChooser.showDirectories(null, "title", "Desc", (JFrame)getTopLevelAncestor(), new ActionListener() {
					@Override
					public void actionPerformed(ActionEvent e) {
						DirectoryChooser t = (DirectoryChooser)e.getSource();
						System.out.println(t.getSelected());
					}
				});
			}
		});
		
		
		setLayout(this);
		add(l_projectdir);
		add(t_projectdir);
		add(b_browse);
	}
	
	@Override public String getName() { return "Edit Configuration"; }
	@Override public String getDescription() { return "Create the software bundle or modify an existing one by setting the target directory"; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public void setVisible(Map<String, Object> properties, boolean isvisible) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean validate(Map<String, Object> properties) {
		return true;
	}
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		l_projectdir.setBounds(x+5, y+5, 120, 25);
		t_projectdir.setBounds(x+125, y+5, w-160, 25);
		b_browse.setBounds(w-35, y+5, 30, 25);
	}

	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
}
