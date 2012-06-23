package org.maxkernel.service.streams;

import java.io.IOException;
import java.nio.channels.Selector;
import java.util.List;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServicePacket;


public interface Stream {
	public enum Mode {
		UNLOCKED, LOCKED
	}
	
	public static final byte GOODBYE		= 0x00;
	//public static final byte AUTH			= 0x01;
	public static final byte HEARTBEAT		= 0x02;
	public static final byte SUBSCRIBE		= 0x03;
	public static final byte UNSUBSCRIBE	= 0x04;
	public static final byte BEGIN			= 0x05;
	public static final byte DATA			= 0x06;
	public static final byte LISTXML		= 0x11;
	
	
	public Mode mode();
	public List<Service> services() throws IOException;
	public void subscribe(Service service) throws IOException;
	public void unsubscribe() throws IOException;
	public void begin(Selector selector) throws IOException;
	public void heartbeat() throws IOException;
	public boolean check();
	public ServicePacket handle() throws IOException;
	public void close() throws IOException;
}
