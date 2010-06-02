package org.installer.wiz;

import java.awt.BorderLayout;
import java.awt.Font;
import java.io.File;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.JProgressBar;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;

import org.installer.OS;
import org.installer.OS.Arch;

public class CopyFilesPage extends WizardPagePart implements Runnable
{
	private static final long serialVersionUID = 0L;
	
	private JProgressBar bar;
	private JTextArea text;
	private boolean isdone;
	
	public CopyFilesPage()
	{
		isdone = false;
		
		bar = new JProgressBar(0,1);
		bar.setStringPainted(true);
		
		text = new JTextArea();
		text.setFont(new Font("sans-serif", Font.PLAIN, 12));
		text.setEditable(false);
		text.setLineWrap(true);
		text.setWrapStyleWord(true);
		text.setTabSize(2);
		
		JScrollPane scroll = new JScrollPane(text, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
		text.setBorder(BorderFactory.createEmptyBorder(3, 3, 3, 3));
		scroll.setBorder(BorderFactory.createEmptyBorder());
		scroll.setViewportBorder(BorderFactory.createEmptyBorder());
		
		setLayout(new BorderLayout());
		add(bar, BorderLayout.NORTH);
		add(scroll, BorderLayout.CENTER);
	}
	
	@Override
	public void setVisible(Map<String, Object> properties, boolean isvisible)
	{
		if (isvisible)
		{
			new Thread(this).start();
		}
	}
	
	@Override
	public void run()
	{
		bar.setIndeterminate(true);
		bar.setString("");
		
		OS os = OS.getOS();
		if (os != OS.UNKNOWN)
		{
			if (OS.getArch() == Arch.x86)
			{
				setText("Locating JVM...");
				File jvmdir = null;
				for (String dir : os.search)
				{
					File f = new File(dir);
					if (f.exists() && f.isDirectory())
					{
						jvmdir = f;
						break;
					}
				}
				
				if (jvmdir == null)
				{
					setText("Could not locate Java install path");
					//TODO - have user select the path
				}
				
				setText("Using JVM at path "+jvmdir.getAbsolutePath());
				
				
				setText("All files installed");
			}
			else
			{
				setError("** Error: 64 bit operating systems are not supported by this installer");
			}
		}
		else
		{
			setError("** Error: Your Operating system cound not be determined");
		}
		
		
		bar.setIndeterminate(false);
		bar.setValue(1);
		isdone = true;
	}
	
	private void setText(String txt)
	{
		text.append(txt+"\n");
		bar.setString(txt);
	}
	
	private void setError(String txt)
	{
		text.append("\n\n"+txt);
		bar.setString("Error");
	}
	
	@Override public String getName() { return "Install files"; }
	@Override public String getDescription() { return "Installing the files..."; }
	@Override public boolean isSkippable() { return false; }
	
	@Override public boolean validate(Map<String, Object> properties) { return isdone; }
}
