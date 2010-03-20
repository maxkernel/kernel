package org.senseta.wiz;

import javax.swing.JFrame;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;

import org.senseta.wiz.Wizard.ViewPolicy;
import org.senseta.wiz.pages.BrowsePage;
import org.senseta.wiz.pages.RunFinalPage;
import org.senseta.wiz.pages.SoftwarePage;
import org.senseta.wiz.pages.WizardPagePart;

public class RunWizard
{
	private static final long serialVersionUID = 0L;
	
	public static void main(String[] args)
	{
		//set default l&f
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (Exception e) {} //ignore
		
		final WizardPagePart[] pages = {
			new BrowsePage(true),
			new SoftwarePage(true),
			new RunFinalPage()
		};
		
		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run()
			{
				JFrame frame = new Wizard("Startup Wizard", pages, ViewPolicy.SIMPLE);
				frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
				
				frame.pack();
				frame.setSize(450, 500);
				frame.setLocationRelativeTo(null);
				frame.setVisible(true);
			}
		});
	}
}
