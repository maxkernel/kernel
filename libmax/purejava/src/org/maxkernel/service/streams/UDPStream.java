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
import java.util.ArrayList;
import java.util.List;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceList;
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
						// On a new data packet
						clear();
						payload = new byte[size];
					}
					
					int numpackets = (size + BODY_SIZE - 1) / BODY_SIZE;
					int offset = packet * BODY_SIZE;
					int length = Math.min(BODY_SIZE, size - offset);
					
					// Position past header
					body.position(HEADER_SIZE);
					body.get(payload, offset, length);
					
					this.numpackets += 1;
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
	
	private Mode mode;
	private long lastheartbeat;
	private Service service;
	
	private DatagramChannel socket;
	private ByteBuffer sendbuf;
	
	public UDPStream(InetSocketAddress sockaddress) throws IOException {
		mode = Mode.UNLOCKED;
		lastheartbeat = 0;
		service = null;
		
		socket = DatagramChannel.open();
		socket.connect(sockaddress);
		sendbuf = ByteBuffer.allocate(SEND_BUFFER_SIZE);
	}
	
	public UDPStream(InetAddress address, int port) throws IOException {
		this(new InetSocketAddress(address, port));
	}
	
	public UDPStream(InetAddress address) throws IOException {
		this(address, DEFAULT_PORT);
	}
	
	@Override
	public Mode mode() {
		return mode;
	}
	
	@Override
	public List<Service> services() throws IOException {
		// TODO - finish me!
		return new ArrayList<Service>();
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
