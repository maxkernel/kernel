package org.senseta.wiz;

import java.awt.Color;

import javax.swing.JFrame;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

import org.senseta.wiz.Wizard.ViewPolicy;
import org.senseta.wiz.pages.CredentialsPage;
import org.senseta.wiz.pages.DeployFinalPage;
import org.senseta.wiz.pages.DeployGeneralPage;
import org.senseta.wiz.pages.DeployReviewPage;
import org.senseta.wiz.pages.BrowsePage;
import org.senseta.wiz.pages.SoftwarePage;
import org.senseta.wiz.pages.WizardPagePart;

public class DeployWizard
{
	private static final long serialVersionUID = 0L;
	
	public static void main(String[] args)
	{
		//set default l&f
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (Exception e) {} //ignore
		
		final Color barcolor = Color.RED;
		final WizardPagePart[] pages = {
			new DeployGeneralPage(),
			new SoftwarePage(false),
			new BrowsePage(false),
			new CredentialsPage(),
			new DeployReviewPage(),
			new DeployFinalPage()
		};
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run()
			{
				Wizard frame = new Wizard("Software Deploy Wizard", pages, ViewPolicy.WIZARD);
				frame.setBarColor(barcolor);
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				
				frame.pack();
				frame.setSize(700, 500);
				frame.setLocationRelativeTo(null);
				frame.setVisible(true);
			}
		});
	}
}
