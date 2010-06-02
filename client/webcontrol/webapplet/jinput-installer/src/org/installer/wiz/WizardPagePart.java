package org.installer.wiz;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import javax.swing.JPanel;

public abstract class WizardPagePart extends JPanel
{
	private static final long serialVersionUID = 0L;
	
	List<ActionListener> actionlisteners;
	public WizardPagePart()
	{
		actionlisteners = new ArrayList<ActionListener>();
	}
	
	public void addActionListener(ActionListener l)
	{
		actionlisteners.add(l);
	}
	
	protected void fireActionEvent(ActionEvent e)
	{
		for (ActionListener l : actionlisteners)
		{
			try {
				l.actionPerformed(e);
			} catch (Exception ex) { ex.printStackTrace(); }
		}
	}
	
	public abstract void setVisible(Map<String, Object> properties, boolean isvisible);
	public abstract boolean validate(Map<String, Object> properties);
	public abstract String getName();
	public abstract String getDescription();
	public abstract boolean isSkippable();
	
	//override this
	public void skipped(Map<String, Object> properties) {};
}
