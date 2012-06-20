package org.maxkernel.service;

import java.io.IOException;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.xml.ws.ProtocolException;

import org.maxkernel.service.streams.TCPStream;

public class ServiceClient {
	public enum StreamType {
		TCP, UDP
	}
	
	public static int DEFAULT_TCP_PORT		= 10001;
	public static int DEFAULT_UDP_PORT		= 10002;
	
	private List<Stream> streams;
	
	public ServiceClient() {
		streams = Collections.synchronizedList(new ArrayList<Stream>());
	}

	public Stream connect(StreamType streamtype, InetAddress address, int port) throws IOException {
		Stream stream = null;
		
		switch (streamtype) {
			case TCP:		stream = new TCPStream(address, port);		break;
			default: throw new ProtocolException("Bad stream type: "+streamtype);
		}
		
		streams.add(stream);
		return stream;
	}
	
	public Stream connect(StreamType streamtype, InetAddress address) throws IOException {
		switch (streamtype) {
			case TCP: return connect(streamtype, address, DEFAULT_TCP_PORT);
			default: throw new ProtocolException("Bad stream type: "+streamtype);
		}
	}
}
