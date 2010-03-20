package org.senseta.wiz;

import java.awt.Color;

import javax.swing.JFrame;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

import org.senseta.wiz.Wizard.ViewPolicy;
import org.senseta.wiz.pages.BundleConfigPage;
import org.senseta.wiz.pages.BundleFinalPage;
import org.senseta.wiz.pages.WizardPagePart;

public class BundleWizard
{
	
	public static void main(String[] args)
	{
		//set default l&f
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (Exception e) {} //ignore
		
		final Color barcolor = Color.BLUE;
		final WizardPagePart[] pages = {
			new BundleConfigPage(),
			new BundleFinalPage()
		};
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run()
			{
				Wizard frame = new Wizard("Bundle Builder", pages, ViewPolicy.WIZARD);
				frame.setBarColor(barcolor);
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				
				frame.pack();
				frame.setSize(600, 600);
				frame.setLocationRelativeTo(null);
				frame.setVisible(true);
			}
		});
	}
}
