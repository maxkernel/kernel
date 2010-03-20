package org.senseta.wiz;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.image.BufferedImage;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextArea;

public class Prompt extends JPanel implements LayoutManager
{
	private static final long serialVersionUID = 0L;
	
	public enum ButtonPolicy { NONE, OKAY, YES_NO, YES_CANCEL, TRYAGAIN };
	public enum Button { NONE, OKAY, YES, NO, CANCEL };
	private static final Logger LOG = Logger.getLogger(Prompt.class.getName());
	private static final BufferedImage[] icons;
	
	public static final int ICON_SUCCESS = 0;
	public static final int ICON_INFO = 1;
	public static final int ICON_QUESTION = 2;
	public static final int ICON_WARNING = 3;
	public static final int ICON_ERROR = 4;
	
	static {
		String[] names = { "success", "info", "question", "warning", "error" };
		icons = new BufferedImage[names.length];
		for (int i=0; i<names.length; i++)
		{
			icons[i] = ImageCache.fromCache(names[i]);
		}
	}
	
	private JTextArea message;
	private JLabel iconlabel;
	private JButton[] buttons;
	private Button lastpressed;
	
	public Prompt(String msg, BufferedImage icon, ButtonPolicy policy)
	{
		lastpressed = Button.NONE;
		
		setBackground(new Color(0xBFBFFF));
		setBorder(BorderFactory.createLineBorder(Color.GRAY));
		
		iconlabel = new JLabel();
		if (icon != null)
		{
			iconlabel.setIcon(new ImageIcon(ImageCache.scale(icon, 50, 50)));
			iconlabel.setVerticalAlignment(JLabel.CENTER);
			iconlabel.setHorizontalAlignment(JLabel.CENTER);
		}
		
		message = new JTextArea(msg);
		message.setFont(new Font("sans-serif", Font.PLAIN, 12));
		message.setOpaque(false);
		message.setEditable(false);
		message.setBorder(null);
		message.setWrapStyleWord(true);
		message.setLineWrap(true);
		
		buttons = makeButtons(policy);
		
		setLayout(this);
		add(message);
		add(iconlabel);
		for (JButton b : buttons)
		{
			add(b);
		}
	}
	
	private JButton[] makeButtons(ButtonPolicy policy)
	{
		JButton[] b;
		
		switch (policy)
		{
			case YES_NO:
			{
				b = new JButton[2];
				b[0] = makeButton("Yes", "sign_tick", Button.YES);
				b[1] = makeButton("No", "sign_cancel", Button.NO);
				break;
			}
			
			case YES_CANCEL:
			{
				b = new JButton[2];
				b[0] = makeButton("Yes", "sign_tick", Button.YES);
				b[1] = makeButton("Cancel", "sign_cancel", Button.CANCEL);
				break;
			}
			
			case TRYAGAIN:
			{
				b = new JButton[1];
				b[0] = makeButton("Try Again?", "okay", Button.OKAY);
				break;
			}
			
			case OKAY:
			{
				b = new JButton[1];
				b[0] = makeButton("Okay", "okay", Button.OKAY);
				break;
			}
			
			case NONE:
			default:
			{
				b = new JButton[0];
				break;
			}
		}
		
		return b;
	}
	
	private JButton makeButton(String text, String icon, final Button button)
	{
		JButton b = new JButton(text, new ImageIcon(ImageCache.fromCache(icon)));
		b.setToolTipText(text);
		b.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				fireActionListener(button, e);
			}
		});
		
		return b;
	}
	
	@Override
	public void requestFocus()
	{
		if (buttons.length > 0)
		{
			buttons[0].requestFocus();
		}
	}
	
	public Button getSelected()
	{
		return lastpressed;
	}
	
	public void addActionListener(ActionListener l)
	{
		listenerList.add(ActionListener.class, l);
	}
	
	public void removeActionListener(ActionListener l)
	{
		listenerList.remove(ActionListener.class, l);
	}
	
	private void fireActionListener(Button button, ActionEvent e)
	{
		lastpressed = button;
		Object[] list = listenerList.getListenerList();
		for (int i=list.length-2; i>=0; i-=2)
		{
			if (list[i] == ActionListener.class)
			{
				try {
					((ActionListener)list[i+1]).actionPerformed(e);
				}
				catch (Throwable t)
				{
					LOG.log(Level.WARNING, "Event listener (ActionListener) threw an exception", t);
				}
			}
		}
	}
	
	
	public static Prompt newConfirm(String text) { return new Prompt(text, icons[ICON_QUESTION], ButtonPolicy.YES_NO); }
	public static Prompt newConfirmCancel(String text) { return new Prompt(text, icons[ICON_QUESTION], ButtonPolicy.YES_CANCEL); }
	public static Prompt newErrorPrompt(String text) { return new Prompt(text, icons[ICON_ERROR], ButtonPolicy.OKAY); }
	public static Prompt newTryAgain(String text) { return new Prompt(text, icons[ICON_ERROR], ButtonPolicy.TRYAGAIN); }
	public static Prompt newMessage(String text, int icon) { return new Prompt(text, icons[icon], ButtonPolicy.NONE); }
	
	@Override
	public void layoutContainer(Container parent)
	{
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		iconlabel.setBounds(x+5, y+5, 50, 50);
		message.setBounds(x+70, y+20, w-70, h-50);
		
		if (buttons.length == 1)
		{
			buttons[0].setBounds(w-115, h-35, 110, 30);
		}
		else
		{
			int offset = w-5-buttons.length*90;
			for (JButton b : buttons)
			{
				b.setBounds(x+offset, h-35, 90, 30);
				offset += 90;
			}
		}
	}

	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
}
