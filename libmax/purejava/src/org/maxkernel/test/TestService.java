package org.maxkernel.test;

import java.io.ByteArrayInputStream;
import java.net.InetAddress;
import java.util.List;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceClient;
import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.streams.Stream;
import org.maxkernel.service.streams.TCPStream;

public class TestService {

	public static void main(String[] args) throws Exception {
		ServiceClient client = new ServiceClient();
		
		Stream stream = new TCPStream(InetAddress.getByName("localhost"));
		
		List<Service> services = stream.services();
		if (services == null) {
			throw new Exception("Services list is NULL!");
		}
		
		//System.out.println("Services: "+services);
		stream.subscribe(services.get(0));
		
		JFrame f = new JFrame();
		f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		f.setSize(320, 240);
		
		JLabel label = new JLabel();
		f.getContentPane().add(label);
		f.setVisible(true);
		
		//System.out.println("Start");
		client.start(stream);
		while (true) {
			ServicePacket p = client.dequeue();
			label.setIcon(new ImageIcon(ImageIO.read(new ByteArrayInputStream(p.data()))));
		}
		
	}
}
