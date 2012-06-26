package org.maxkernel.test;

import java.awt.Image;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ObjectInputStream;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.SwingConstants;

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
		
		JLabel label = new JLabel("", SwingConstants.CENTER);
		f.getContentPane().add(label);
		f.setVisible(true);
		
		
		System.out.println("Start");
		client.start(stream);
		while (true) {
			ServicePacket p = client.dequeue();
			
			//ByteBuffer b = ByteBuffer.wrap(p.data()).order(ByteOrder.LITTLE_ENDIAN);
			//System.out.println(b.getDouble() + "\t" + b.getDouble()+ "\t" + b.getDouble());
			BufferedImage img = ImageIO.read(new ByteArrayInputStream(p.data()));
			label.setIcon(new ImageIcon(img.getScaledInstance(img.getWidth() * 2, img.getHeight() * 2, Image.SCALE_FAST)));
		}
		
	}
}
