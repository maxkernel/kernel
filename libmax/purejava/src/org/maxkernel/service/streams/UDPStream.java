package org.maxkernel.service.streams;

import java.io.EOFException;
import java.io.IOException;
import java.io.InvalidObjectException;
import java.io.Reader;
import java.io.SyncFailedException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.DatagramChannel;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.util.ArrayList;
import java.util.List;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceList;
import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.streams.Stream.Mode;
import org.xml.sax.SAXException;

public class UDPStream implements Stream {
	private static class Packet {
		private static final int HEADER_SIZE = 17;
		private static final int BODY_SIZE = 512 - HEADER_SIZE;
		
		private static final int CODE_OFFSET = 0;
		private static final int TIMESTAMP_OFFSET = 1;
		private static final int SIZE_OFFSET = 9;
		private static final int PACKET_OFFSET = 13;
		
		private ByteBuffer body;
		
		private byte code;
		private long timestamp;
		private int size;
		
		private byte[] payload;
		private int numpackets;
		
		public Packet() {
			body = ByteBuffer.allocate(HEADER_SIZE + BODY_SIZE).order(ByteOrder.LITTLE_ENDIAN);
			clear();
		}
		
		public boolean read(ReadableByteChannel channel) throws IOException {
			body.clear();
			int read = channel.read(body);
			body.flip();
			
			if (read < 0) {
				throw new EOFException();
				
			} else if (read < 1) {
				// Did not even send 1 byte of data
				return false;
				
			}
			
			this.code = body.get(CODE_OFFSET);
			switch (code) {
				case Stream.DATA: {
					if (read < HEADER_SIZE)
					{
						return false;
					}
					
					long timestamp = body.getLong(TIMESTAMP_OFFSET);
					int size = body.getInt(SIZE_OFFSET);
					int packet = body.getInt(PACKET_OFFSET);
					
					if (timestamp != this.timestamp || size != this.size)
					{
						//int numpackets = (size + BODY_SIZE - 1) / BODY_SIZE;
						//System.out.println("New frame "+this.numpackets+" / "+numpackets);
						
						// On a new data packet
						clear();
						this.timestamp = timestamp;
						this.size = size;
						payload = new byte[size];
					}
					
					int numpackets = (size + BODY_SIZE - 1) / BODY_SIZE;
					int offset = packet * BODY_SIZE;
					int length = Math.min(BODY_SIZE, size - offset);
					
					// Position past header
					body.position(HEADER_SIZE);
					body.get(payload, offset, length);
					
					this.numpackets += 1;
					//System.out.println("PACKET "+this.numpackets +" / "+ numpackets);
					
					return numpackets == this.numpackets;
				}
				
				case Stream.HEARTBEAT:
				case Stream.GOODBYE:
				default:
					return true;
			}
		}
		
		public void clear() { body.clear(); code = 0; timestamp = 0; size = 0; payload = null; numpackets = 0; }
		public byte code() { return code; }
		public long timestamp() { return timestamp; }
		public int size() { return size; }
		public byte[] payload() { return payload; }
	}
	
	public static final int DEFAULT_PORT = 10002;

	private static final int SEND_BUFFER_SIZE = 128;
	private static final int HEARTBEAT_TIMEOUT = 5000;
	private static final long LIST_TIMEOUT = 250;
	
	private Mode mode;
	private long lastheartbeat;
	private Service service;
	
	private DatagramChannel socket;
	private ByteBuffer sendbuf;
	
	private Packet packet;
	
	public UDPStream(InetSocketAddress sockaddress) throws IOException {
		mode = Mode.UNLOCKED;
		lastheartbeat = 0;
		service = null;
		
		socket = DatagramChannel.open();
		socket.connect(sockaddress);
		socket.configureBlocking(false);
		sendbuf = ByteBuffer.allocate(SEND_BUFFER_SIZE);
		
		packet = new Packet();
	}
	
	public UDPStream(InetAddress address, int port) throws IOException {
		this(new InetSocketAddress(address, port));
	}
	
	public UDPStream(InetAddress address) throws IOException {
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
		
		synchronized (socket) {
			
			Selector selector = Selector.open();
			
			try {
				
				socket.register(selector, SelectionKey.OP_READ);
				send(new byte[]{ Stream.LISTXML });
				
				do {
					
					if (selector.select(LIST_TIMEOUT) == 0) {
						throw new IOException("Could not read all services data!");
					}
					
				} while (!packet.read(socket));
				
				selector.close();
				
				
				try {
					return ServiceList.parseXML(new Reader() {
						int left = packet.size();
						
						@Override
						public int read(char[] cbuf, int off, int len) throws IOException {
							if (left == 0) {
								return -1;
							}
							
							int amount = Math.min(len, left);
							char[] array = new String(packet.payload(), packet.size() - left, amount).toCharArray();
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
				packet.clear();
				selector.close();
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
		
			if (!packet.read(socket)) {
				return null;
			}
			
			switch (packet.code()) {
				case Stream.HEARTBEAT: {
					lastheartbeat = System.currentTimeMillis();
					return null;
				}
				
				case Stream.DATA: {
					ServicePacket servicepacket = new ServicePacket(service, this, packet.timestamp(), packet.payload());
					packet.clear();
					
					return servicepacket;
				}
				
				default: {
					throw new InvalidObjectException("Unknown code received from service server: "+packet.code());
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
