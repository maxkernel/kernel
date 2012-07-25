package org.maxkernel.test;

import java.awt.Image;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ObjectInputStream;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.SwingConstants;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceClient;
import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.format.BufferedImageFormat;
import org.maxkernel.service.format.DoubleArrayFormat;
import org.maxkernel.service.streams.Stream;
import org.maxkernel.service.streams.TCPStream;
import org.maxkernel.service.streams.UDPStream;

public class TestService {

	public static void main(String[] args) throws Exception {
		/*
		JFrame f = new JFrame();
		f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		f.setSize(320, 240);
		
		JLabel label = new JLabel("", SwingConstants.CENTER);
		f.getContentPane().add(label);
		f.setVisible(true);
		f.setExtendedState(JFrame.MAXIMIZED_BOTH);
		*/
		
		
		ServiceClient client = new ServiceClient();
		
		//Stream tcpstream = new TCPStream(InetAddress.getByName("192.168.1.100"));
		//Stream udpstream = new UDPStream(InetAddress.getByName("192.168.1.100"));
		Stream stream = new TCPStream(InetAddress.getByName("localhost"));
		
		Map<String, Service> services = stream.services();
		if (services == null) {
			throw new Exception("Services list is NULL!");
		}
		
		System.out.println("Services: "+services);
		//stream.subscribe(services.get("Camera video stream"));
		stream.subscribe(services.get("dstream"));
		
		//BlockingQueue<ServicePacket> imgqueue = new LinkedBlockingQueue<ServicePacket>();
		//BufferedImageFormat imgformat = new BufferedImageFormat(imgqueue);
		//client.begin(stream, imgqueue);
		
		BlockingQueue<ServicePacket> dqueue = new LinkedBlockingQueue<ServicePacket>();
		DoubleArrayFormat dformat = new DoubleArrayFormat(dqueue);
		client.begin(stream, dqueue);
		
		while (true) {
			//BufferedImage img = imgformat.dequeue().payload();
			
			//ByteBuffer b = ByteBuffer.wrap(p.data()).order(ByteOrder.LITTLE_ENDIAN);
			System.out.println(dformat.dequeue().data()[0]);
			
			//final int blowup = 3;
			
			//BufferedImage img = ImageIO.read(new ByteArrayInputStream(p.data()));
			//label.setIcon(new ImageIcon(img.getScaledInstance(img.getWidth() * blowup, img.getHeight() * blowup, Image.SCALE_FAST)));
			//label.setIcon(new ImageIcon(imgformat.dequeue().data()));
			//System.out.println(Arrays.toString(dformat.dequeue().data()));
		}
		
	}
}
