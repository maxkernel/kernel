package org.webcontrol.applet.ui;

import java.awt.CardLayout;

import javax.swing.JButton;
import javax.swing.JPanel;

import org.webcontrol.applet.ui.util.CenteredLayout;
import org.webcontrol.applet.ui.util.DragArea;
import org.webcontrol.applet.ui.widget.MLabel;
import org.webcontrol.applet.ui.widget.MWindow;

public class ContentPanel extends JPanel
{
	private static final String INITIALIZE = "initalize";
	private static final String ERROR = "error";
	
	private CardLayout layout;
	private MLabel initialize;
	private MLabel error;
	
	
	
	public ContentPanel()
	{
		setOpaque(false);
		
		initialize = new MLabel("Initializing...");
		error = new MLabel("Error: (no error)");
		
		setLayout(layout = new CardLayout());
		add(initialize, INITIALIZE);
		//add(new CenteredLayout(new DragArea(new MWindow("Foobarbaz"), 50, 50, 200, 150), 500, 500), "FOOBAR");
		add(new MainPanel(), "FOOBAR");
		add(error, ERROR);
		
		layout.show(this, "FOOBAR");
	}
}
