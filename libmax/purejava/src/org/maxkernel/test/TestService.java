package org.maxkernel.test;

import java.io.IOException;
import java.net.InetAddress;
import java.util.List;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceClient;
import org.maxkernel.service.ServiceClient.StreamType;
import org.maxkernel.service.Stream;

public class TestService {

	public static void main(String[] args) throws Exception {
		ServiceClient client = new ServiceClient();
		
		Stream stream = client.connect(StreamType.TCP, InetAddress.getByName("localhost"));
		if (stream == null) {
			throw new IOException("Could not connect to SERVICE!");
		}
		
		List<Service> services = stream.services();
		if (services == null) {
			throw new Exception("Services list is NULL!");
		}
		System.out.println("Services: "+services);
		
		stream.subscribe(services.get(0).getName());
		stream.subscribe("foobar");
		
		stream.close();
	}
}
