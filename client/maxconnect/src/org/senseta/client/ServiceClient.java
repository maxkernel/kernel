package org.senseta.client;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.DatagramChannel;
import java.nio.channels.SelectableChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.nio.channels.WritableByteChannel;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.senseta.event.ServiceListener;

public class ServiceClient
{
	private static final Logger LOG = Logger.getLogger(ServiceClient.class.getName());
	
	public static enum Protocol {
		TCP(0),
		UDP(1);
		
		private int index;
		private Protocol(int i)
		{
			index = i;
		}
	};
	
	private static final int KEEPALIVE_SLEEP = 3000;
	private final ByteBuffer KEEPALIVE_PACKET = makePacket(null, "__alive", null, 0, new byte[0]);
	private final ByteBuffer LIST_PACKET = makePacket(null, "__list", null, 0, new byte[]{'l','i','s','t',0});
	private final ByteBuffer REG_PACKET = makePacket(null, "__list", null, 0, new byte[100]);
	private final ByteBuffer ECHO_PACKET = makePacket(null, "__echo", null, 0, new byte[50]);
	
	private static final int HEADER_LENGTH = 40;
	private static final int HANDLE_MAXLEN = 8;
	private static final short PAYLOAD_MAXLEN = 10000;
	private static final byte[] SYNC = { (byte)0xFF, (byte)0xFF, (byte)0xFF, (byte)0xFF, (byte)0xFF, (byte)0xFF, (byte)0xFF, (byte)0xFF };
	private static final byte ZERO = (byte)0;
	
	private static final int REPLY_TIMEOUT = 500;
	private Lock listlock, echolock;
	private StringBuffer listbuffer, echobuffer;
	
	private InetAddress host;
	private String client_handle;
	
	private Selector selector;
	private Stream streams[];
	
	private Thread readthread;
	private Thread keepalive;
	
	private Map<String, List<ServiceListener>> listeners;
	
	
	public ServiceClient(InetAddress host) throws IOException
	{
		this.host = host;
		streams = new Stream[Protocol.values().length];
		streams[Protocol.TCP.index] = new TCPStream();
		streams[Protocol.UDP.index] = new UDPStream();
		listlock = new ReentrantLock(true);
		echolock = new ReentrantLock(true);
		listbuffer = new StringBuffer();
		echobuffer = new StringBuffer();
		listeners = Collections.synchronizedMap(new HashMap<String, List<ServiceListener>>());
	}
	
	private void dispatchFrame(String service_handle, Protocol p, long timestamp_us, byte[] data)
	{
		if (service_handle.equals("__list"))
		{
			synchronized (listbuffer) {
				listbuffer.append(new String(data));
				listbuffer.notifyAll();
			}
			return;
		}
		else if (service_handle.equals("__echo"))
		{
			synchronized (echobuffer) {
				echobuffer.append(new String(data));
				echobuffer.notifyAll();
			}
			return;
		}
		
		List<ServiceListener> list = listeners.get(service_handle + "_" + p.toString());
		if (list != null)
		{
			for (ServiceListener l : list)
			{
				try
				{
					l.newFrame(service_handle, p, timestamp_us, data);
				} catch (Throwable t)
				{
					LOG.log(Level.SEVERE, "ServiceListener threw an exception in the newFrame function", t);
				}
			}
		}
	}
	
	public void openStream(Protocol p, int port) throws IllegalStateException, IOException
	{
		if (streams[p.index].isOpen())
		{
			throw new IllegalStateException("Service client already has opened "+p.toString()+" stream");
		}
		
		if (selector == null)
		{
			selector = Selector.open();
		}
		
		streams[p.index].connect(new InetSocketAddress(host, port));
		streams[p.index].getSelectableChannel().register(selector, SelectionKey.OP_READ, streams[p.index]);
	}
	
	public void startStream(String service_handle, Protocol p, ServiceListener listener) throws IOException, IllegalStateException, IllegalArgumentException
	{
		if (!streams[p.index].isOpen())
		{
			throw new IllegalStateException("Cannot use unopened stream ("+p.toString()+") to list services");
		}
		
		if (readthread == null)
		{
			throw new IllegalStateException("Must start the read thread before listing services");
		}
		
		byte[] data = new byte[7+service_handle.length()];
		System.arraycopy(new byte[]{'s','t','a','r','t','='}, 0, data, 0, 6);
		for (int i=0; i<service_handle.length(); i++)
		{
			data[6+i] = (byte)service_handle.charAt(i);
		}
		data[data.length-1] = '\0';
		
		setPacketClient(REG_PACKET, client_handle);
		setPacketPayload(REG_PACKET, data, 0, data.length);
		REG_PACKET.position(0);
		
		if (listener != null)
		{
			addServiceListener(service_handle, p, listener);
		}
		
		streams[p.index].write(REG_PACKET);
	}
	
	public void addServiceListener(String service_handle, Protocol p, ServiceListener listener) throws IllegalArgumentException
	{
		if (service_handle.startsWith("__"))
		{
			throw new IllegalArgumentException("Cannot add ServiceListener to service "+service_handle+", that service is locked");
		}
		
		String key = service_handle + "_" + p.toString();
		List<ServiceListener> list = listeners.get(key);
		
		if (list == null)
		{
			list = Collections.synchronizedList(new ArrayList<ServiceListener>());
			listeners.put(key, list);
		}
		
		list.add(listener);
	}
	
	public void closeStream(Protocol p) throws IOException
	{
		if (!streams[p.index].isOpen())
		{
			return;
		}
		
		streams[p.index].getSelectableChannel().keyFor(selector).cancel();
		streams[p.index].close();
	}
	
	public void listServices(Protocol p) throws IOException
	{
		if (!streams[p.index].isOpen())
		{
			throw new IllegalStateException("Cannot use unopened stream ("+p.toString()+") to list services");
		}
		
		if (readthread == null)
		{
			throw new IllegalStateException("Must start the read thread before listing services");
		}
		
		listlock.lock();
		try
		{
			listbuffer.setLength(0);
			
			setPacketClient(LIST_PACKET, client_handle);
			LIST_PACKET.position(0);
			streams[p.index].write(LIST_PACKET);
			
			try
			{
				synchronized (listbuffer) {
					listbuffer.wait(REPLY_TIMEOUT);
				}
			} catch (InterruptedException e) {} //ignore
			
			System.out.println("data: "+listbuffer.toString());
		} finally
		{
			listlock.unlock();
		}
	}
	
	public Long echo(Protocol p, long data) throws IOException
	{
		if (!streams[p.index].isOpen())
		{
			throw new IllegalStateException("Cannot use unopened stream ("+p.toString()+") to list services");
		}
		
		if (readthread == null)
		{
			throw new IllegalStateException("Must start the read thread before listing services");
		}
		
		echolock.lock();
		try
		{
			echobuffer.setLength(0);
			echobuffer.append(data);
			byte[] ts = echobuffer.toString().getBytes();
			echobuffer.setLength(0);
			
			setPacketClient(ECHO_PACKET, client_handle);
			setPacketPayload(ECHO_PACKET, ts, 0, ts.length);
			ECHO_PACKET.position(0);
			streams[p.index].write(ECHO_PACKET);
			
			try
			{
				synchronized (echobuffer) {
					echobuffer.wait(REPLY_TIMEOUT);
				}
			} catch (InterruptedException e) {} //ignore
			
			Long value = null;
			try
			{
				value = Long.parseLong(echobuffer.toString().trim());
			} catch (NumberFormatException ex) {} //ignore
			
			return value;
		} finally
		{
			echolock.unlock();
		}
	}
	
	public void startReadThread()
	{
		if (readthread != null)
		{
			return;
		}
		
		readthread = new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					while (true)
					{
						selector.select();
						if (Thread.interrupted())
						{
							break;
						}
						
						for (SelectionKey key : selector.selectedKeys())
						{
							if (key.isValid() && key.isReadable())
							{
								try {
									Stream stream = (Stream)key.attachment();
									stream.newdata();
								} catch (Exception e)
								{
									LOG.log(Level.WARNING, "Exception thrown when reading stream data", e);
									key.cancel();
								}
							}
						}
					}
					
				} catch (IOException e)
				{
					LOG.log(Level.WARNING, "IOException was thrown in service read thread", e);
				}
				
				readthread = null;
			}
		});
		
		readthread.setDaemon(true);
		readthread.start();
	}
	
	public void stopReadThread()
	{
		stopThread(readthread);
	}
	
	public void startKeepaliveThread(final Protocol ... p)
	{
		if (keepalive != null)
		{
			return;
		}
		
		for (Protocol pr : p)
		{
			if (!streams[pr.index].isOpen())
			{
				throw new IllegalStateException("Cannot use unopened stream ("+pr.toString()+") as client keep-alive");
			}
		}
		
		keepalive = new Thread(new Runnable() {
			@Override
			public void run() {
				try {
					while (true)
					{
						Thread.sleep(KEEPALIVE_SLEEP);
						if (Thread.interrupted())
						{
							break;
						}
						
						if (client_handle != null)
						{
							setPacketClient(KEEPALIVE_PACKET, client_handle);
							
							for (Protocol pr : p)
							{
								KEEPALIVE_PACKET.position(0);
								streams[pr.index].write(KEEPALIVE_PACKET);
							}
						}
					}
				} catch (IOException e)
				{
					LOG.log(Level.WARNING, "IOException was thrown in the service keep-alive thread", e);
				} catch (InterruptedException e)
				{
					//do nothing, this is loop breakout point
				}
				
				keepalive = null;
			}
		});
		
		keepalive.setDaemon(true);
		keepalive.start();
	}
	
	public void stopKeepaliveThread()
	{
		stopThread(keepalive);
	}
	
	private static void stopThread(Thread t)
	{
		if (t == null)
		{
			return;
		}
		t.interrupt();
	}
	
	private static ByteBuffer makePacket(ByteBuffer buf, String service_handle, String client_handle, long timestamp_ms, byte[] data) throws IllegalArgumentException
	{
		if (data.length > PAYLOAD_MAXLEN)
		{
			throw new IllegalArgumentException("Packet payload length too big, max length = "+PAYLOAD_MAXLEN);
		}
		
		if (buf == null || buf.capacity() < (data.length + HEADER_LENGTH))
		{
			buf = ByteBuffer.allocate(data.length + HEADER_LENGTH);
		}
		
		buf.clear();
		buf.order(ByteOrder.LITTLE_ENDIAN);
		
		//add sync field
		buf.put(SYNC);
		
		//add service_handle field
		for (int i=0; i<(HANDLE_MAXLEN - 1); i++)
		{
			if (service_handle.length() > i)
			{
				buf.put((byte)service_handle.charAt(i));
			}
			else
			{
				buf.put(ZERO);
			}
		}
		buf.put(ZERO);
		
		//add client_handle field
		setPacketClient(buf, client_handle);
		buf.position(buf.position()+8);
		
		//add size fields
		buf.putInt(data.length);		//payload size
		buf.putShort(PAYLOAD_MAXLEN);	//fragmentation size
		buf.putShort(ZERO);				//fragmentation number
		
		//write timestamp (timestamp is in microseconds)
		buf.putLong(timestamp_ms * 1000L);
		
		//write data
		buf.put(data);
		buf.flip();
		
		return buf;
	}
	
	private static void setPacketClient(ByteBuffer buf, String client_handle)
	{
		if (client_handle == null)
		{
			client_handle = "";
		}
		
		for (int i=0; i<(HANDLE_MAXLEN - 1); i++)
		{
			if (client_handle.length() > i)
			{
				buf.put(16 + i, (byte)client_handle.charAt(i));
			}
			else
			{
				buf.put(16 + i, ZERO);
			}
		}
		buf.put(23, ZERO);
	}
	
	private static void setPacketPayload(ByteBuffer buf, byte[] data, int offset, int length) throws ArrayIndexOutOfBoundsException
	{
		buf.putInt(24, length);
		System.arraycopy(data, offset, buf.array(), HEADER_LENGTH, length);
		buf.limit(HEADER_LENGTH + length);
	}
	
	private static Header parseHeader(ByteBuffer buf)
	{
		for (int i=0; i<SYNC.length; i++)
		{
			if (buf.get(i) != SYNC[i])
			{
				return null;
			}
		}
		
		StringBuilder sh = new StringBuilder();
		for (int i=0; i<HANDLE_MAXLEN; i++)
		{
			char c = (char)buf.get(8 + i);
			if (c == 0)
			{
				break;
			}
			
			sh.append(c);
		}
		
		StringBuilder ch = new StringBuilder();
		for (int i=0; i<HANDLE_MAXLEN; i++)
		{
			char c = (char)buf.get(16 + i);
			if (c == 0)
			{
				break;
			}
			
			ch.append(c);
		}
		
		Header h = new Header();
		h.service_handle = sh.toString();
		h.client_handle = ch.toString();
		h.framelength = buf.getInt(24);
		h.fragsize = buf.getShort(28);
		h.fragnum = buf.getShort(30);
		h.timestamp_us = buf.getLong(32);
		
		return h;
	}
	
	private void setClient(String client_handle) throws IllegalStateException
	{
		if (this.client_handle == null)
		{
			this.client_handle = client_handle;
		}
		else if (!this.client_handle.equals(client_handle))
		{
			throw new IllegalStateException("Cannot overwright client_handle "+this.client_handle+" with "+client_handle);
		}
	}
	
	private interface Stream extends WritableByteChannel {
		public void connect(SocketAddress address) throws IOException;
		public SelectableChannel getSelectableChannel();
		public void newdata() throws IOException, IllegalArgumentException, IllegalStateException;
	}
	
	private class TCPStream implements Stream
	{
		private SocketChannel channel;
		private ByteBuffer header, payload;
		private FrameAssembler assembler;
		
		private TCPStream() throws IOException
		{
			header = ByteBuffer.allocate(HEADER_LENGTH).order(ByteOrder.LITTLE_ENDIAN);
			payload = ByteBuffer.allocate(PAYLOAD_MAXLEN).order(ByteOrder.LITTLE_ENDIAN);
			assembler = new FrameAssembler(true);
			channel = SocketChannel.open();
			channel.configureBlocking(false);
		}
		
		@Override
		public void connect(SocketAddress address) throws IOException
		{
			channel.connect(address);
			if (!channel.finishConnect())
			{
				throw new IOException("Could not open TCP connection to "+address.toString());
			}
		}
		
		@Override
		public void newdata() throws IOException
		{
			header.position(0);
			channel.read(header);
			Header h = parseHeader(header);
			
			if (h == null)
			{
				throw new IOException("Error parsing TCP header");
			}
			
			setClient(h.client_handle);
			
			payload.position(0);
			payload.limit(FrameAssembler.payloadsize(h));
			channel.read(payload);
			
			byte[] frame = assembler.assemble(h, payload.array(), 0, payload.position());
			
			if (frame != null)
			{
				dispatchFrame(h.service_handle, Protocol.TCP, h.timestamp_us, frame);
			}
		}
		
		@Override public SelectableChannel getSelectableChannel() { return channel; }
		@Override public int write(ByteBuffer src) throws IOException { return channel.write(src); }
		@Override public void close() throws IOException { channel.close(); }
		@Override public boolean isOpen() { return channel.isConnected(); }
		
	}
	
	private class UDPStream implements Stream
	{
		private DatagramChannel channel;
		private ByteBuffer packet;
		private FrameAssembler assembler;
		
		private UDPStream() throws IOException
		{
			packet = ByteBuffer.allocate(HEADER_LENGTH + PAYLOAD_MAXLEN).order(ByteOrder.LITTLE_ENDIAN);
			assembler = new FrameAssembler(false);
			channel = DatagramChannel.open();
			channel.configureBlocking(false);
		}
		
		@Override
		public void connect(SocketAddress address) throws IOException
		{
			channel.connect(address);
		}

		@Override
		public void newdata() throws IOException, IllegalArgumentException, IllegalStateException
		{
			packet.position(0);
			Header h = parseHeader(packet);
			
			if (h == null)
			{
				throw new IOException("Error parsing UDP header");
			}
			
			setClient(h.client_handle);
			byte[] frame = assembler.assemble(h, packet.array(), HEADER_LENGTH, packet.position() - HEADER_LENGTH);
			
			if (frame != null)
			{
				dispatchFrame(h.service_handle, Protocol.UDP, h.timestamp_us, frame);
			}
		}

		@Override public SelectableChannel getSelectableChannel() { return channel; }
		@Override public int write(ByteBuffer src) throws IOException { return channel.write(src); }
		@Override public void close() throws IOException { channel.close(); }
		@Override public boolean isOpen() { return channel.isConnected(); }
		
	}
	
	private static class Header
	{
		private String service_handle;
		private String client_handle;
		private int framelength;
		private int fragsize;
		private int fragnum;
		private long timestamp_us;
	}
	
	private static class Frame
	{
		private int fragcount;
		private long timestamp;
		private byte[] data;
	}
	
	private static class FrameAssembler
	{
		private Map<String, Frame> frames;
		private boolean failondrop;
		
		private FrameAssembler(boolean failondrop)
		{
			frames = Collections.synchronizedMap(new HashMap<String, Frame>());
			this.failondrop = failondrop;
		}
		
		public byte[] assemble(Header h, byte[] data, int offset, int length) throws IllegalArgumentException, IllegalStateException
		{
			Frame f = frames.get(h.service_handle);
			if (f != null && f.timestamp != h.timestamp_us)
			{
				frames.remove(h.service_handle);
				f = null;
				
				if (failondrop)
				{
					throw new IllegalStateException("Frame dropped. (Service "+h.service_handle+")");
				}
			}
			
			if (f == null)
			{
				f = new Frame();
				f.fragcount = 0;
				f.timestamp = h.timestamp_us;
				f.data = new byte[h.framelength];
				
				if (h.framelength > h.fragsize)
				{
					frames.put(h.service_handle, f);
				}
			}
			
			int numfrags = h.framelength / h.fragsize;
			if ((h.framelength % h.fragsize) > 0)
			{
				numfrags += 1;
			}
			
			int bufoffset = h.fragsize * h.fragnum;
			int buflen = h.fragnum == (numfrags - 1) && (h.framelength % h.fragsize) > 0? h.framelength % h.fragsize : h.fragsize;
			if (length != buflen)
			{
				throw new IllegalArgumentException("Unexpected packet length. (size="+h.framelength+", numfrags="+numfrags+", fragnum="+h.fragnum+") (expected "+buflen+" got "+length+")");
			}
			
			System.out.println("(size="+h.framelength+", numfrags="+numfrags+", fragnum="+h.fragnum+")");
			
			System.arraycopy(data, offset, f.data, bufoffset, buflen);
			if (++f.fragcount == numfrags)
			{
				frames.remove(h.service_handle);
				return f.data;
			}
			
			return null;
		}
		
		public static final int payloadsize(Header h)
		{	
			return (h.fragnum + 1) * h.fragsize > h.framelength? h.framelength % h.fragsize : h.fragsize;
		}
	}
}
