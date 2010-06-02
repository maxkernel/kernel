package org.installer;

import java.awt.Color;

import javax.swing.JFrame;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

import org.installer.wiz.CopyFilesPage;
import org.installer.wiz.FinishPage;
import org.installer.wiz.GeneralPage;
import org.installer.wiz.Wizard;
import org.installer.wiz.WizardPagePart;
import org.installer.wiz.Wizard.ViewPolicy;

public class JInputInstaller
{
	public static void main(String[] args)
	{
		//set default l&f
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (Exception e) {} //ignore
		
		final Color barcolor = Color.BLUE;
		final WizardPagePart[] pages = {
			new GeneralPage(),
			new CopyFilesPage(),
			new FinishPage()
		};
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run()
			{
				Wizard frame = new Wizard("Startup Wizard", pages, ViewPolicy.WIZARD);
				frame.setBarColor(barcolor);
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				
				frame.pack();
				frame.setSize(570, 450);
				frame.setLocationRelativeTo(null);
				frame.setVisible(true);
			}
		});
	}
}
