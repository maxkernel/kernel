package org.senseta.wiz.pages;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Toolkit;
import java.awt.datatransfer.StringSelection;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.File;
import java.io.FileOutputStream;
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
import org.senseta.wiz.task.DeployTask;
import org.senseta.wiz.task.TaskListener;

public class DeployFinalPage extends WizardPagePart implements LayoutManager, TaskListener<Void>
{
	private static final long serialVersionUID = 0L;
	
	private DeployTask task;
	
	private JProgressBar bar;
	private JScrollPane scroll;
	private JTextArea text;
	private JButton save, copy;
	private Prompt prompt;
	
	private boolean isdone;
	
	public DeployFinalPage()
	{
		prompt = null;
		
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
		text.setBorder(null);
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
		text.setText("");
		text.setCaretPosition(0);
		
		task.start();
	}
	
	@Override
	public void setVisible(Map<String, Object> map, boolean isvisible)
	{
		if (isvisible)
		{
			MaxRobot robot = (MaxRobot)map.get("robot");
			SoftwareBundle software = (SoftwareBundle)map.get("software");
			String username = (String)map.get("username");
			String password = (String)map.get("password");
			
			task = new DeployTask(robot, software, username, password, this);
			startTask();
		}
	}

	@Override public String getName() { return "Install"; }
	@Override public String getDescription() { return "The text below is from the automated installer. " +
			"Please ensure there are no errors and have a nice day!"; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public void taskAppendOutput(String str)
	{
		text.append(str);
		text.setCaretPosition(text.getText().length());
		
		if (str.endsWith("...\n"))
		{
			bar.setString(str.substring(0, str.length()-1));
		}
	}

	@Override
	public void taskShowPrompt(Prompt prompt)
	{
		prompt.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				clearPrompt();
			}
		});
		showPrompt(prompt);
	}
	
	@Override
	public void taskComplete(Void value, boolean iserror)
	{
		isdone = true;
		
		bar.setIndeterminate(false);
		bar.setValue(1);
		bar.setString(iserror? "Error" : "Complete");
		
		if (iserror)
		{
			taskAppendOutput("** Error: "+task.getError().getMessage());
			Prompt p = Prompt.newTryAgain(task.getError().getMessage());
			p.addActionListener(new ActionListener() {
				@Override
				public void actionPerformed(ActionEvent e)
				{
					clearPrompt();
					startTask();
				}
			});
			showPrompt(p);
		}
		else
		{
			showPrompt(Prompt.newMessage("Success!\nSoftware has been deployed.", Prompt.ICON_SUCCESS));
		}
	}
	
	private void showPrompt(Prompt p)
	{
		if (prompt != null)
		{
			clearPrompt();
		}
		
		prompt = p;
		add(prompt);
		p.requestFocus();
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

	//@Override
	//public void run()
	//{
		/*
		appendln("Connecting to rover ... (this may take some time)");
		
		try {
			
			JSch jsch = new JSch();
			Session session = jsch.getSession(username, host, 22);
			session.setUserInfo(new UserInfo() {
				@Override public void showMessage(String msg) { /*System.out.println(msg);* / }
				@Override public boolean promptYesNo(String msg) { /*System.out.println("(Y/N) "+msg);* / return true; }
				@Override public boolean promptPassword(String arg0) { return true; }
				@Override public String getPassword() { return password; }
				@Override public boolean promptPassphrase(String arg0) { return true; }
				@Override public String getPassphrase() { return null; }
			});
			
			session.connect(30000);
			appendln("Success! Connected to "+session.getServerVersion());
			
			ChannelShell shell = (ChannelShell)session.openChannel("shell");
			shell.setInputStream(new ByteArrayInputStream(("mkdir "+tempdir+"\nexit 0\n").getBytes()));
			shell.connect();
			
			Thread.sleep(500);
			
			ChannelSftp sftp = (ChannelSftp)session.openChannel("sftp");
			SftpProgressMonitor mtr = new SftpProgressMonitor() {
				@Override
				public void init(int arg0, String arg1, String arg2, long len)
				{
					append("Uploading "+filename+": [");
				}
				
				public boolean count(long on)
				{
					append("#");
					return true;
				}

				@Override public void end() {
					appendln("#]");
				}
			};
			
			sftp.connect();
			sftp.put(filepath, tempdir, mtr, ChannelSftp.OVERWRITE);
			Thread.sleep(500);
			
			appendln("Extracting archive");
			
			shell = (ChannelShell)session.openChannel("shell");
			shell.setInputStream(new ByteArrayInputStream(("cd "+tempdir+"\nunzip "+filename+"\nexit 0\n").getBytes()));
			shell.connect();
			
			Thread.sleep(500);
			
			appendln("Executing installer =======================");
			shell = (ChannelShell)session.openChannel("shell");
			ByteArrayInputStream in = null;
			if (bundletype.equals("application"))
				in = new ByteArrayInputStream(("cd "+tempdir+"\necho [S\nbash master.install.bash\necho E]\nexit 0\n").getBytes());
			else if (bundletype.equals("upgrade"))
				in = new ByteArrayInputStream(("cd "+tempdir+"\necho [S\necho '"+password+"' | sudo -S bash master.upgrade.bash\necho E]\nexit 0\n").getBytes());
			else
				in = new ByteArrayInputStream(("echo [S\necho '** Error: Don't know what to do with bundletype: "+bundletype+"'\necho E]\nexit 0\n").getBytes());
			
			shell.setInputStream(in);
			ByteArrayOutputStream out = new ByteArrayOutputStream();
			shell.setOutputStream(out);
			shell.connect();
			
			Thread.sleep(500);
			
			boolean echo = false;
			boolean done = false;
			int loop = 0;
			
			while (loop < 200)
			{
				String str = out.toString();
				out.reset();
				
				//don't show password
				str = str.replace("'"+password+"'", "'****'");
				
				//get rid of some weird artifacts
				str = str.replace("[C", "").replace("[A", "");
				
				int start = -1;
				int end = -1;
				
				if (!echo && (start = str.indexOf(" echo [S")) != -1)
				{
					echo = true;
					start += 14;
				}
				if (start < 0) start = 0;
				
				if (echo && (end = str.indexOf(" echo E]", start)) != -1)
				{
					done = true;
				}
				if (end < 0) end = str.length();
				
				if (str.length() > 0 && echo) append(str.substring(start, end));
				if (done) break;
				
				Thread.sleep(100);
				loop++;
			}
			
			Thread.sleep(500);
			
			appendln("\n==================================");
			appendln("Cleaning up");
			shell = (ChannelShell)session.openChannel("shell");
			shell.setInputStream(new ByteArrayInputStream(("rm -r "+tempdir+"\nexit 0\n").getBytes()));
			shell.connect();
			
			Thread.sleep(500);
			appendln("Done");
			
			sftp.disconnect();
			session.disconnect();
		} catch (Exception e) {
			appendln("\n** Error: "+e.getMessage());
		}
		*/
	//	isdone = true;
	//}
}
