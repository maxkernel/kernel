package org.senseta.wiz.task;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.util.Random;

import org.senseta.MaxRobot;
import org.senseta.SoftwareBundle;
import org.senseta.SoftwareCache;

import com.jcraft.jsch.ChannelSftp;
import com.jcraft.jsch.ChannelShell;
import com.jcraft.jsch.JSch;
import com.jcraft.jsch.Session;
import com.jcraft.jsch.UserInfo;

public class DeployTask extends Task<Void>
{
	private static final int SSH_PORT = 22;
	
	private MaxRobot robot;
	private SoftwareBundle bundle;
	private String username, password;
	
	public DeployTask(MaxRobot robot, SoftwareBundle bundle, String username, String password, TaskListener<Void> listener)
	{
		super(listener);
		
		this.robot = robot;
		this.bundle = bundle;
		this.username = username;
		this.password = password;
	}

	@Override
	public Void call() throws Exception
	{
		String tempdir = "/tmp/"+bundle.getName()+"-"+Double.toString(bundle.getVersion())+".extract."+(new Random(System.currentTimeMillis()).nextInt(10000))+"/";
		File file = SoftwareCache.getFile(bundle);
		String filepath = file.getAbsolutePath();
		String filename = file.getName();
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		
		appendln("Connecting to rover...");
		appendln("(This may take some time)");
		try {
			
			JSch jsch = new JSch();
			Session session = jsch.getSession(username, robot.getIP(), SSH_PORT);
			session.setUserInfo(new UserInfo() {
				@Override public void showMessage(String msg) { /*System.out.println(msg);*/ }
				@Override public boolean promptYesNo(String msg) { /*System.out.println("(Y/N) "+msg);*/ return true; }
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
			Thread.sleep(1000);
			
			
			appendln("Uploading...");
			ChannelSftp sftp = (ChannelSftp)session.openChannel("sftp");
			sftp.connect();
			sftp.put(filepath, tempdir);
			Thread.sleep(1000);
			
			
			appendln("Extracting archive...");
			appendln("==================================");
			
			shell = (ChannelShell)session.openChannel("shell");
			shell.setInputStream(new ByteArrayInputStream(("cd "+tempdir+"\nunzip "+filename+" 2>&1\nexit 0\n").getBytes()));
			out.reset();
			shell.setOutputStream(out);
			shell.connect();
			
			Thread.sleep(1000);
			appendln(out.toString());
			
			
			appendln("Executing installer...");
			appendln("==================================");
			shell = (ChannelShell)session.openChannel("shell");
			ByteArrayInputStream in = null;
			switch (bundle.getType())
			{
				case APPLICATION:
					in = new ByteArrayInputStream(("cd "+tempdir+"\necho [S\nbash master.install.bash\necho E]\nexit 0\n").getBytes());
					break;
				
				case UPGRADE:
					in = new ByteArrayInputStream(("cd "+tempdir+"\necho [S\necho '"+password+"' | sudo -S bash master.upgrade.bash\necho E]\nexit 0\n").getBytes());
					break;
				
				default:
					in = new ByteArrayInputStream(("echo [S\necho '** Error: Don't know what to do with bundletype: "+bundle.getType().toString().toLowerCase()+"'\necho E]\nexit 0\n").getBytes());
					break;
			}
			
			shell.setInputStream(in);
			out.reset();
			shell.setOutputStream(out);
			shell.connect();
			
			Thread.sleep(1000);
			
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
				
				if (str.length() > 0 && echo) appendln(str.substring(start, end));
				if (done) break;
				
				Thread.sleep(100);
				loop++;
			}
			
			Thread.sleep(1000);
			
			appendln("\n==================================");
			appendln("Cleaning up...");
			shell = (ChannelShell)session.openChannel("shell");
			shell.setInputStream(new ByteArrayInputStream(("rm -r "+tempdir+"\nexit 0\n").getBytes()));
			shell.connect();
			
			Thread.sleep(1000);
			appendln("Done");
			
			sftp.disconnect();
			session.disconnect();
			
			return null;
		} catch (Exception e) {
			throw new Exception("Error during deployment.\nReason: "+e.getMessage(), e);
		}
	}
}
