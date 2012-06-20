package org.maxkernel.service.streams;

import java.io.IOException;
import java.io.InvalidObjectException;
import java.io.Reader;
import java.io.SyncFailedException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.SocketChannel;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceList;
import org.maxkernel.service.Stream;
import org.xml.sax.SAXException;

public class TCPStream implements Stream {
	private static final Logger LOG = Logger.getLogger(TCPStream.class.getCanonicalName());
	private static int SEND_BUFFER_SIZE = 128;
	private static int RECV_BUFFER_SIZE = 256;
	
	private Mode mode = Mode.UNLOCKED;
	
	private SocketChannel socket;
	private ByteBuffer sendbuf, recvbuf;

	public TCPStream(InetAddress address, int port) throws IOException {
		socket = SocketChannel.open();
		socket.connect(new InetSocketAddress(address, port));
		
		sendbuf = ByteBuffer.allocate(SEND_BUFFER_SIZE);
		recvbuf = ByteBuffer.allocate(RECV_BUFFER_SIZE);
		sendbuf.order(ByteOrder.LITTLE_ENDIAN);
		recvbuf.order(ByteOrder.LITTLE_ENDIAN);
	}

	private synchronized void send(byte[] data) throws IOException {
		sendbuf.clear();
		sendbuf.put(data);
		sendbuf.flip();
		
		while (sendbuf.hasRemaining()) {
			socket.write(sendbuf);
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
		
		recvbuf.clear();
		recvbuf.limit(13);			// Only read in the header data here (uint8, int64, uint32)
		socket.read(recvbuf);
		recvbuf.flip();
		
		final byte code = recvbuf.get();			// Receive the code (should be DATA)
		recvbuf.position(recvbuf.position() + 8);	// Skip past the 64bit timestamp
		final int size = recvbuf.getInt();			// Receive the data size
		
		if (code != Stream.DATA) {
			throw new InvalidObjectException("Bad return code from service server: "+code);
		}
		
		try {
			return ServiceList.parseXML(new Reader() {
				int readleft = size;
				
				@Override
				public int read(char[] cbuf, int off, int len) throws IOException {
					if (readleft == 0) {
						return -1;
					}
					
					if (recvbuf.remaining() == 0) {
						recvbuf.clear();
						recvbuf.limit(Math.min(readleft, recvbuf.capacity()));
						socket.read(recvbuf);
						recvbuf.flip();
					}
					
					byte[] bytedata = new byte[Math.min(recvbuf.remaining(), len)];
					recvbuf.get(bytedata);
					readleft -= bytedata.length;
					
					char[] chardata = new String(bytedata).toCharArray();
					System.arraycopy(chardata, 0, cbuf, off, chardata.length);
					return chardata.length;
				}
				
				@Override
				public void close() throws IOException {
					// Ignore it
				}
			});
		} catch (SAXException e) {
			throw new InvalidObjectException("XML parse exception. Bad data: "+e.getMessage());
		}
	}
	
	@Override
	public void subscribe(String name) throws IOException {
		if (mode() != Mode.UNLOCKED) {
			throw new SyncFailedException("Attempting to subscribe from locked stream!");
		}
		
		byte[] bytename = name.getBytes("UTF8");
		
		byte[] data = new byte[1 + bytename.length + 1];	// subscribe_code + service_name + \0
		Arrays.fill(data, (byte)0);
		data[0] = Stream.SUBSCRIBE;
		System.arraycopy(bytename, 0, data, 1, bytename.length);
		
		send(data);
	}
	
	@Override
	public void unsubscribe() throws IOException {
		if (mode() != Mode.UNLOCKED) {
			throw new SyncFailedException("Attempting to unsubscribe from locked stream!");
		}
		
		send(new byte[]{ Stream.UNSUBSCRIBE });
	}

	@Override
	public void close() throws IOException {
		try {
			send(new byte[]{ Stream.GOODBYE });
		} finally {
			socket.close();
		}
	}
}
