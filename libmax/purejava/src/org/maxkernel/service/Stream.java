package org.maxkernel.service;

import java.io.IOException;
import java.util.List;


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
	public void subscribe(String name) throws IOException;
	public void unsubscribe() throws IOException;
	
	public void close() throws IOException;
}
