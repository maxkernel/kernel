package org.senseta.wiz.pages;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Toolkit;
import java.awt.Window;
import java.awt.datatransfer.StringSelection;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JOptionPane;
import javax.swing.JProgressBar;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.border.EtchedBorder;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;
import org.senseta.wiz.ImageCache;
import org.senseta.wiz.Prompt;
import org.senseta.wiz.task.ExecuteTask;
import org.senseta.wiz.task.SoftwareSyncTask;
import org.senseta.wiz.task.Task;
import org.senseta.wiz.task.TaskListener;
import org.senseta.wiz.ui.ProcessStarter;

public class RunFinalPage extends WizardPagePart implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	private JProgressBar bar;
	private JTextArea text;
	private JScrollPane scroll;
	private JButton save, copy;
	private Prompt prompt;
	
	//private MaxRobot robot;
	private Task<?> task;
	private boolean isdone;
	
	public RunFinalPage()
	{
		bar = new JProgressBar(0,1);
		bar.setStringPainted(true);
		
		copy = new JButton(new ImageIcon(ImageCache.fromCache("copy")));
		copy.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				Toolkit.getDefaultToolkit().getSystemClipboard().setContents(new StringSelection(text.getText()), null);
				text.requestFocus();
				text.selectAll();
			}
		});
		
		save = new JButton(new ImageIcon(ImageCache.fromCache("save")));
		save.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				JFileChooser chooser = new JFileChooser();
				chooser.setMultiSelectionEnabled(false);
				if (chooser.showOpenDialog(getTopLevelAncestor()) == JFileChooser.APPROVE_OPTION)
				{
					File fp = chooser.getSelectedFile();
					if (fp.exists() && JOptionPane.showConfirmDialog(getTopLevelAncestor(), "Are you sure you want to overwrite "+fp.getName()+"?", "Overwrite?", JOptionPane.YES_NO_OPTION, JOptionPane.QUESTION_MESSAGE) == JOptionPane.YES_OPTION)
						fp.delete();
					
					try {
						
						FileOutputStream fout = new FileOutputStream(fp);
						fout.write(text.getText().getBytes());
						fout.flush();
						fout.close();
						
					} catch (Exception ex) {
						String message = "Could not save file\nReason: "+ex.getMessage();
						Prompt p = Prompt.newErrorPrompt(message);
						p.addActionListener(new ActionListener() {
							@Override
							public void actionPerformed(ActionEvent e)
							{
								clearPrompt();
							}
						});
						showPrompt(p);
					}
				}
			}
		});
		
		text = new JTextArea("");
		text.setFont(new Font("sans-serif", Font.PLAIN, 12));
		text.setEditable(false);
		text.setTabSize(2);
		text.setWrapStyleWord(true);
		text.setLineWrap(true);
		
		scroll = new JScrollPane(text, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		text.setBorder(BorderFactory.createEmptyBorder(3, 5, 3, 3));
		scroll.setBorder(BorderFactory.createEtchedBorder(EtchedBorder.LOWERED));
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		setLayout(this);
		add(bar);
		add(copy);
		add(save);
		add(scroll);
	}
	
	private void startTask()
	{
		isdone = false;
		bar.setIndeterminate(true);
		bar.setString("");
		
		task.start();
	}

	@Override
	public void setVisible(Map<String, Object> properties, boolean isvisible)
	{
		if (isvisible)
		{
			MaxRobot robot = (MaxRobot)properties.get("robot");
			SoftwareBundle software = (SoftwareBundle)properties.get("software");
			
			if (robot == null && software != null)
			{
				step2(null, software);
			}
			else
			{
				step1(robot);
			}
		}
	}
	
	private void step1(final MaxRobot robot)
	{
		task = new SoftwareSyncTask(robot, new TaskListener<SoftwareBundle>() {
			@Override
			public void taskComplete(SoftwareBundle bundle, boolean iserror)
			{
				if (iserror)
				{
					setError(task.getError().getMessage());
					return;
				}
				
				step2(robot, bundle);
			}
			
			@Override public void taskAppendOutput(String text) { append(text); }
			@Override public void taskShowPrompt(Prompt prompt) { showPrompt(prompt); }
		});
		startTask();
	}
	
	private void step2(final MaxRobot robot, final SoftwareBundle bundle)
	{
		task = new ExecuteTask(robot, bundle, new TaskListener<ProcessBuilder>() {
			@Override
			public void taskComplete(ProcessBuilder pb, boolean iserror)
			{
				if (iserror)
				{
					setError(task.getError().getMessage());
					return;
				}
				
				try
				{
					//launch the process starter frame
					ProcessStarter.start(pb, robot, bundle);
				} catch (IOException e)
				{
					setError("Could not start up.\nReason: "+e.getMessage());
					return;
				}
				
				//dispose the top frame
				((Window)getTopLevelAncestor()).dispose();
			}
			
			@Override public void taskAppendOutput(String text) { append(text); }
			@Override public void taskShowPrompt(Prompt prompt) { showPrompt(prompt); }
		});
		startTask();
	}
	
	public void append(String str)
	{
		if (str.endsWith("...\n"))
		{
			bar.setString(str.substring(0, str.length()-1));
		}
		
		text.append(str);
		text.setCaretPosition(text.getText().length());
	}
	
	private void showPrompt(Prompt p)
	{
		if (prompt != null)
		{
			clearPrompt();
		}
		
		p.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				clearPrompt();
			}
		});
		
		prompt = p;
		add(prompt);
		prompt.requestFocus();
		revalidate();
	}
	
	private void clearPrompt()
	{
		if (prompt == null)
		{
			return;
		}
		
		remove(prompt);
		prompt = null;
		revalidate();
	}
	
	private void setError(String text)
	{
		isdone = true;
		append("**Error: "+text+"\n");
		
		bar.setIndeterminate(false);
		bar.setValue(1);
		bar.setString("Error");
		
		Prompt p = Prompt.newTryAgain(text);
		p.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				append("Retrying...\n");
				startTask();
			}
		});
		showPrompt(p);
	}
	
	@Override public String getName() { return "Startup"; }
	@Override public String getDescription() { return "This will sync the software and launch the process"; }
	@Override public boolean isSkippable() { return false; }
	
	@Override
	public boolean validate(Map<String, Object> properties)
	{
		return isdone;
	}

	

	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		bar.setBounds(x, y, w-50, 25);
		copy.setBounds(w-50, y, 25, 25);
		save.setBounds(w-25, y, 25, 25);
		
		if (prompt == null)
		{
			scroll.setBounds(x, y+25, w, h-25);
		}
		else
		{
			scroll.setBounds(x, y+25, w, h-125);
			prompt.setBounds(x, h-100, w, 100);
		}
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
