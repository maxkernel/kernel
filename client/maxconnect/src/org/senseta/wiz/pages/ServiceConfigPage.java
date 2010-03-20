package org.senseta.wiz.pages;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Map;

import javax.swing.BorderFactory;
import javax.swing.ButtonGroup;
import javax.swing.JCheckBox;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JTextField;
import javax.swing.border.Border;

import org.senseta.MaxRobot;
import org.senseta.client.ServiceClient.Protocol;
import org.senseta.wiz.uipart.ComponentTitledBorder;

public class ServiceConfigPage extends WizardPagePart implements LayoutManager
{
	private static final int TCP_DEFAULT = 10001;
	private static final int UDP_DEFAULT = 10002;
	
	
	private StreamConfig tcp, udp;
	private JLabel warning;
	
	public ServiceConfigPage()
	{
		tcp = new StreamConfig(Protocol.TCP, TCP_DEFAULT, true);
		udp = new StreamConfig(Protocol.UDP, UDP_DEFAULT, false);
		
		warning = new JLabel("", JLabel.CENTER);
		warning.setForeground(Color.RED);
		warning.setVisible(false);
		
		setLayout(this);
		add(tcp);
		add(udp);
		add(warning);
	}

	@Override public String getName() { return "Service Configuration"; }
	@Override public String getDescription() { return "Review the service configuration parameters"; }
	@Override public boolean isSkippable() { return false; }

	@Override
	public void setVisible(Map<String, Object> properties, boolean isvisible) {
		if (isvisible)
		{
			MaxRobot r = (MaxRobot)properties.get("robot");
			String tcpport = r.get("service_tcpport");
			String udpport = r.get("service_udpport");
			
			try
			{
				if (tcpport != null)
					tcp.setPort(Integer.parseInt(tcpport));
				
				if (udpport != null)
					udp.setPort(Integer.parseInt(udpport));
				
			} catch (NumberFormatException e) {} //ignore
		}
	}

	@Override
	public boolean validate(Map<String, Object> properties) {
		Protocol deflt = null;
		
		if (!tcp.isDisabled())
		{
			if (!tcp.doValidate())
				return false;
			
			if (tcp.isDefault())
				deflt = Protocol.TCP;
			
			properties.put("tcpport", tcp.getPort());
		}
		else
		{
			properties.remove("tcpport");
		}
		
		if (!udp.isDisabled())
		{
			if (!udp.doValidate())
				return false;
			
			if (udp.isDefault())
				deflt = Protocol.UDP;
			
			properties.put("udpport", udp.getPort());
		}
		else
		{
			properties.remove("udpport");
		}
		
		if (deflt == null)
		{
			warning.setText("Must have a default protocol");
			warning.setVisible(true);
			return false;
		}
		
		properties.put("default_protocol", deflt);
		
		warning.setVisible(false);
		return true;
	}
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		warning.setBounds(x+5, y+5, w, 20);
		tcp.setBounds(x+5, y+25, w-10, 90);
		udp.setBounds(x+5, y+120, w-10, 90);
	}

	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	
	private static class StreamConfig extends JPanel implements LayoutManager
	{
		private static ButtonGroup defaultgroup = new ButtonGroup();
		
		private JLabel portlabel;
		private JTextField port;
		private JRadioButton asdefault;
		private JCheckBox enabled;
		
		private StreamConfig(Protocol p, int defaultport, boolean isdefault)
		{
			enabled = new JCheckBox(p.toString(), true);
			enabled.setOpaque(true);
			enabled.addActionListener(new ActionListener() {
				@Override
				public void actionPerformed(ActionEvent e) {
					port.setEnabled(enabled.isSelected());
					asdefault.setEnabled(enabled.isSelected());
				}
			});
			
			Border b = new ComponentTitledBorder(enabled, this, BorderFactory.createEtchedBorder());
			
			portlabel = new JLabel(p.toString()+" port", JLabel.RIGHT);
			port = new JTextField(""+defaultport);
			
			asdefault = new JRadioButton("Use as default");
			defaultgroup.add(asdefault);
			
			if (isdefault)
			{
				asdefault.setSelected(true);
			}
			
			setBorder(b);
			setLayout(this);
			add(portlabel);
			add(port);
			add(asdefault);
		}
		
		private boolean doValidate()
		{
			if (!isDisabled())
			{
				try
				{
					int p = Integer.parseInt(port.getText());
					
					if (p <= 0 || p >= ((int)Short.MAX_VALUE * 2))
						throw new NumberFormatException("Invalid port number");
				} catch (NumberFormatException e)
				{
					portlabel.setForeground(Color.RED);
					return false;
				}
			}
			
			portlabel.setForeground(Color.BLACK);
			return true;
		}
		
		private void setPort(int newport)
		{
			port.setText(""+newport);
		}
		
		private int getPort()
		{
			return Integer.parseInt(port.getText());
		}
		
		private boolean isDefault()
		{
			return !isDisabled() && defaultgroup.isSelected(asdefault.getModel());
		}
		
		private boolean isDisabled()
		{
			return !enabled.isSelected();
		}
		
		@Override
		public void layoutContainer(Container parent) {
			Insets in = parent.getInsets();
			int x = in.left, y = in.top;
			int w = parent.getWidth() - in.left - in.right;
			int h = parent.getHeight() - in.top - in.bottom;
			
			portlabel.setBounds(x+5, y+5, 75, 25);
			port.setBounds(x+90, y+5, 100, 25);
			asdefault.setBounds(x+20, y+35, 200, 25);
		}

		@Override public void addLayoutComponent(String name, Component comp) {}
		@Override public void removeLayoutComponent(Component comp) {}
		@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
		@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	}
}
