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
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

import javax.imageio.ImageIO;
import javax.swing.ImageIcon;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.SwingConstants;

import org.maxkernel.service.DisconnectListener;
import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceClient;
import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.queue.BufferedImageServiceQueue;
import org.maxkernel.service.queue.DoubleArrayServiceQueue;
import org.maxkernel.service.queue.ServiceQueue;
import org.maxkernel.service.streams.Stream;
import org.maxkernel.service.streams.TCPStream;
import org.maxkernel.service.streams.UDPStream;

public class TestService {

	public static void main(String[] args) throws Exception {
		
		// Create the service client
		ServiceClient client = new ServiceClient();
		client.addDisconnectListener(new DisconnectListener() {
			@Override
			public void streamDisconnected(DisconnectEvent e) {
				System.out.println(String.format("Stream disconnected! (%s)", e.getStream()));
			}
		});
		
		// Create our TCPStream as before
		Stream tcp_stream = new TCPStream(InetAddress.getByName("localhost")); // Replace with robot IP
		
		// Get the service named 'gps'
		Map<String, Service> services = tcp_stream.listServices();
		Service gps_service = services.get("network");  // TODO - check for null
		
		// Subscribe to the 'gps' service
		tcp_stream.subscribe(gps_service);
		
		// Create our blocking queue that the raw gps data will go into
		BlockingQueue<ServicePacket<byte[]>> gps_raw_queue = new LinkedBlockingQueue<>();
		
		// The blocking queue will contain ServicePackets of type byte[] (the default format).
		// The ServiceClient is unable to automatically convert the service data.
		// In order to convert the data, we must provide a proper converting class.
		// In this example, we know, however, that the 'gps' service outputs double[]
		// So, we will use a DoubleArrayServiceQueue to convert the raw byte[] service
		// data into a usable double[]
		ServiceQueue<double[]> gps_queue = ServiceQueue.make(double[].class, gps_raw_queue); //new DoubleArrayServiceQueue(gps_raw_queue);
		
		// Start the streaming. The service client will put ServicePackets received
		// by tcp_stream into the gps_packet queue
		client.begin(tcp_stream, gps_queue);
		
		// Loop over 10 packets
		for (int i=0; i<10; i++) {
			ServicePacket<double[]> gps_packet = gps_queue.take();
			
			long timestamp_us = gps_packet.getTimestamp();  // microsecond timestamp
			double[] data = gps_packet.getData();
			
			Date sample_time = new Date(timestamp_us / 1000);
			System.out.println(String.format("Lat: %f, Long: %f, Time: %s", data[0], /*data[1]*/ 1.0, sample_time));
		}
		
		// Close the ServiceClient (will close all registered streams too)
		client.close();
		
		/*
		for (Service s : services.values()) {
			String name = s.getName();
			String format = s.getFormat();
			String description = s.getDescription();
			
			System.out.println(String.format("%s - (format = %s) %s", name, format, description));
		}
		*/
		
		/*
		JFrame f = new JFrame();
		f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		f.setSize(320, 240);
		
		JLabel label = new JLabel("", SwingConstants.CENTER);
		f.getContentPane().add(label);
		f.setVisible(true);
		f.setExtendedState(JFrame.MAXIMIZED_BOTH);
		
		
		ServiceClient client = new ServiceClient();
		Stream stream = new TCPStream(InetAddress.getByName("localhost"));
		
		Map<String, Service> services = stream.getServices();
		if (services == null) {
			throw new Exception("Services list is NULL!");
		}
		
		System.out.println("Services: "+services);
		stream.subscribe(services.get("network"));
		
		//BlockingQueue<ServicePacket> imgqueue = new LinkedBlockingQueue<ServicePacket>();
		//BufferedImageFormat imgformat = new BufferedImageFormat(imgqueue);
		//client.begin(stream, imgqueue);
		
		BlockingQueue<ServicePacket<byte[]>> queue = new LinkedBlockingQueue<ServicePacket<byte[]>>();
		DoubleArrayServiceQueue format = new DoubleArrayServiceQueue(queue);
		client.begin(stream, format);
		
		while (true) {
			//BufferedImage img = imgformat.dequeue().payload();
			
			//ByteBuffer b = ByteBuffer.wrap(p.data()).order(ByteOrder.LITTLE_ENDIAN);
			System.out.println(format.take().getData()[0]);
			
			//final int blowup = 3;
			
			//BufferedImage img = ImageIO.read(new ByteArrayInputStream(p.data()));
			//label.setIcon(new ImageIcon(img.getScaledInstance(img.getWidth() * blowup, img.getHeight() * blowup, Image.SCALE_FAST)));
			//label.setIcon(new ImageIcon(imgformat.dequeue().data()));
			//System.out.println(Arrays.toString(dformat.dequeue().data()));
		}
		*/
	}
}
