package org.maxkernel.service.streams;

import java.io.IOException;
import java.io.InvalidObjectException;
import java.io.Reader;
import java.io.SyncFailedException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.util.List;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceList;
import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.priv.packet.CodePacket;
import org.maxkernel.service.priv.packet.DataPacket;
import org.xml.sax.SAXException;

public class TCPStream implements Stream {
	public static final int DEFAULT_PORT = 10001;
	
	private static final int SEND_BUFFER_SIZE = 128;
	private static final int HEARTBEAT_TIMEOUT = 5000;
	
	private Mode mode;
	private long lastheartbeat;
	private Service service;
	
	private SocketChannel socket;
	private ByteBuffer sendbuf;
	
	private CodePacket code;
	private DataPacket data;
	
	public TCPStream(InetSocketAddress sockaddress) throws IOException {
		mode = Mode.UNLOCKED;
		lastheartbeat = 0;
		service = null;
		
		socket = SocketChannel.open();
		socket.connect(sockaddress);
		sendbuf = ByteBuffer.allocate(SEND_BUFFER_SIZE);
		
		code = new CodePacket();
		data = new DataPacket();
	}

	public TCPStream(InetAddress address, int port) throws IOException {
		this(new InetSocketAddress(address, port));
	}
	
	public TCPStream(InetAddress address) throws IOException {
		this(address, DEFAULT_PORT);
	}

	private void send(byte[] data) throws IOException {
		// TODO - make sure that data can fit into sendbuf
		// TODO - make smarter!! (split data into multiple passes of sendbuf!)
		
		synchronized (socket) {
			sendbuf.clear();
			sendbuf.put(data);
			sendbuf.flip();
			
			if (socket.write(sendbuf) != data.length) {
				throw new IOException("Could not send all data to server");
			}
		}
	}
	
	@Override
	public Mode mode() {
		return mode;
	}
	
	@Override
	public List<Service> services() throws IOException {
		if (mode() != Mode.UNLOCKED) {
			throw new SyncFailedException("Attempting to get service list from locked stream!");
		}
		
		send(new byte[]{ Stream.LISTXML });
		
		synchronized (socket) {
		
			try {
				if (!code.read(socket)) {
					throw new IOException("Could not read all services data!");
				}
				
				if (code.code() != Stream.DATA) {
					throw new InvalidObjectException("Bad return code from service server: "+code.code());
				}
			} finally {
				code.clear();
			}
			
			try {
				if (!data.read(socket)) {
					throw new IOException("Could not read all services data!");
				}
			
				try {
					return ServiceList.parseXML(new Reader() {
						int left = data.payload().length;
						
						@Override
						public int read(char[] cbuf, int off, int len) throws IOException {
							if (left == 0) {
								return -1;
							}
							
							int amount = Math.min(len, left);
							char[] array = new String(data.payload(), data.payload().length - left, amount).toCharArray();
							left -= amount;
							
							System.arraycopy(array, 0, cbuf, off, array.length);
							return array.length;
						}
						
						@Override
						public void close() throws IOException {
							// Ignore it
						}
					});
				} catch (SAXException e) {
					throw new InvalidObjectException("XML parse exception. Bad data: "+e.getMessage());
				}
				
			} finally {
				data.clear();
			}
		}
	}
	
	@Override
	public void subscribe(Service service) throws IOException {
		if (mode() != Mode.UNLOCKED) {
			throw new SyncFailedException("Bad stream state: "+mode());
		}
		
		this.service = service;
		
		String name = service.getName();
		int length = name.length();
		
		byte[] data = new byte[length + 2];
		data[0] = Stream.SUBSCRIBE;
		System.arraycopy(name.getBytes("UTF8"), 0, data, 1, length);
		data[length+1] = 0;
		send(data);
	}
	
	@Override
	public void unsubscribe() throws IOException {
		if (mode() != Mode.UNLOCKED) {
			throw new SyncFailedException("Bad stream state: "+mode());
		}
		
		send(new byte[]{ Stream.UNSUBSCRIBE });
	}
	
	@Override
	public void begin(Selector selector) throws IOException {
		if (mode() != Mode.UNLOCKED) {
			throw new SyncFailedException("Bad stream state: "+mode());
		}
		
		// Lock the stream
		this.lastheartbeat = System.currentTimeMillis();
		this.mode = Mode.LOCKED;
		send(new byte[]{ Stream.BEGIN });
		
		// Set up the selector
		socket.configureBlocking(false);
		socket.register(selector, SelectionKey.OP_READ, this);
	}
	
	@Override
	public void heartbeat() throws IOException {
		if (mode != Mode.LOCKED) {
			return;
		}
		
		// Don't try to throttle the heartbeat rates
		send(new byte[]{ Stream.HEARTBEAT });
	}
	
	@Override
	public boolean check() {
		if (mode() != Mode.LOCKED) {
			return true;
		}
		
		return (this.lastheartbeat + HEARTBEAT_TIMEOUT) > System.currentTimeMillis();
	}
	
	@Override
	public ServicePacket handle() throws IOException {
		if (mode() != Mode.LOCKED) {
			throw new SyncFailedException("Bad stream state: "+mode());
		}
		
		synchronized (socket) {
		
			if (!code.read(socket)) {
				return null;
			}
			
			switch (code.code()) {
				case Stream.HEARTBEAT: {
					lastheartbeat = System.currentTimeMillis();
					code.clear();
					return null;
				}
				
				case Stream.DATA: {
					if (!data.read(socket)) {
						return null;
					}
					
					ServicePacket packet = new ServicePacket(service, this, data.timestamp(), data.payload());
					code.clear();
					data.clear();
					
					return packet;
				}
				
				default: {
					throw new InvalidObjectException("Unknown code received from service server: "+code.code());
				}
			}
		}
	}

	@Override
	public void close() throws IOException {
		try {
			send(new byte[]{ Stream.GOODBYE });
		} catch (IOException ex) {
			// Ignore it. Just close the stream
		} finally {
			socket.close();
		}
	}
}
