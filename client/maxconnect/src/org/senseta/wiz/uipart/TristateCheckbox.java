package org.senseta.wiz.uipart;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.util.EventListener;

import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JLabel;
import javax.swing.JPanel;

import org.senseta.wiz.ImageCache;

public class TristateCheckbox extends JPanel implements LayoutManager, MouseListener {
	private static final long serialVersionUID = -4404437875938358320L;
	
	public enum CheckState {
		UNCHECKED(new ImageIcon(ImageCache.fromCache("check_off")), new ImageIcon(ImageCache.fromCache("check_hover"))),
		PARTIAL(new ImageIcon(ImageCache.fromCache("check_partial")), new ImageIcon(ImageCache.fromCache("check_hover"))),
		CHECKED(new ImageIcon(ImageCache.fromCache("check_on")), new ImageIcon(ImageCache.fromCache("check_hover")));
		
		public static CheckState next(CheckState current)
		{
			if (current == UNCHECKED || current == PARTIAL)
			{
				return CHECKED;
			}
			return UNCHECKED;
		}
		
		private ImageIcon normal, hover;
		private CheckState(ImageIcon icon, ImageIcon hover)
		{
			this.normal = icon;
			this.hover = hover;
		}
	}
	
	private boolean mouseover;
	private CheckState state;
	private JLabel check, text;
	
	public TristateCheckbox(CheckState state, String text, Icon icon)
	{
		mouseover = false;
		
		this.state = state;
		this.check = new JLabel(state.normal);
		this.text = new JLabel(text, icon, JLabel.LEFT);
		
		setOpaque(false);
		addMouseListener(this);
		
		setLayout(this);
		add(this.check);
		add(this.text);
	}
	
	public TristateCheckbox()
	{
		this(CheckState.UNCHECKED, "", null);
	}
	
	public CheckState getState()
	{
		return state;
	}
	
	public void setState(CheckState newstate)
	{
		state = newstate;
		check.setIcon(newstate.normal);
	}
	
	public void setText(String text) { this.text.setText(text); }
	public void setIcon(Icon icon) { text.setIcon(icon); }
	
	public void addActionListener(ActionListener listener) { listenerList.add(ActionListener.class, listener); }
	public void removeActionListener(ActionListener listener) { listenerList.remove(ActionListener.class, listener); }
	
	protected void fireEvent(ActionEvent e)
	{
		EventListener listeners[] = listenerList.getListeners(ActionListener.class);
	    for (EventListener l : listeners) {
	      ((ActionListener) l).actionPerformed(e);
	    }
	}

	@Override
	public void mousePressed(MouseEvent e) {
		check.setIcon(state.hover);
	}

	@Override
	public void mouseReleased(MouseEvent e) {
		check.setIcon(state.normal);
		if (mouseover)
		{
			state = CheckState.next(state);
			
			fireEvent(new ActionEvent(this, e.getID(), "stateChanged"));
			check.setIcon(state.normal);
		}
	}
	
	@Override public void mouseEntered(MouseEvent e) { mouseover = true; }
	@Override public void mouseExited(MouseEvent e) { mouseover = false; }
	@Override public void mouseClicked(MouseEvent e) {}
	
	@Override
	public Dimension getPreferredSize()
	{
		Dimension td = text.getPreferredSize();
		return new Dimension(td.width + 25, Math.max(20, td.height));
	}
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		check.setBounds(x, y, 20, h);
		text.setBounds(x+20, y, w-20, h);
	}

	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
}
