package org.maxkernel.syscall;

import java.io.EOFException;
import java.io.IOException;
import java.io.InvalidClassException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ReadableByteChannel;
import java.nio.channels.SocketChannel;
import java.nio.charset.Charset;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.activation.UnsupportedDataTypeException;

public class SyscallClient {
	private static final class ReturnPacket {
		private static final int SIZE = 128;
		
		private static final int FRAMEING_OFFSET = 0;
		private static final int TYPE_OFFSET = 4;
		private static final int SYSCALL_OFFSET = 5;
		
		private ByteBuffer packet;
		
		private String syscall;
		private String signature;
		private Object data;
		
		public ReturnPacket() {
			packet = ByteBuffer.allocate(SIZE).order(ByteOrder.LITTLE_ENDIAN);
			packet.clear();
		}
		
		private String readstring(int offset) {
			StringBuffer buffer = new StringBuffer();
			
			while (offset < packet.limit()) {
				char c = (char)packet.get(offset++);
				if (c == '\0') {
					return buffer.toString();
				}
				
				buffer.append(c);
			}
			
			return null;
		}
		
		public boolean read(ReadableByteChannel channel) throws IOException {
			int read = channel.read(packet);
			if (read < 0) {
				throw new EOFException();
			} else if (read == 0) {
				throw new BufferUnderflowException();
			}
			
			if (packet.position() < SYSCALL_OFFSET) {
				return false;
			}
			
			if (packet.getInt(FRAMEING_OFFSET) != CONSOLE_FRAMEING) {
				throw new IOException("Bad syscall message framing");
			}
			
			syscall = readstring(SYSCALL_OFFSET);
			if (syscall == null) {
				return false;
			}
			
			signature = readstring(SYSCALL_OFFSET + syscall.length() + 1);
			if (signature == null) {
				return false;
			}
			
			if (signature.length() != 1) {
				throw new IOException("Unknown reply message signature format");
			}
			
			char sig = signature.charAt(0);
			int offset = SYSCALL_OFFSET + syscall.length() + 3;
			
			try {
				switch (sig) {
					case TYPE_VOID:
						break;
						
					case TYPE_BOOLEAN:
						data = (packet.get(offset) != 0)? true : false;
						offset += 1;
						break;
					
					case TYPE_INTEGER:
						data = packet.getInt(offset);
						offset += 4;
						break;
						
					case TYPE_DOUBLE:
						data = packet.getDouble(offset);
						offset += 8;
						break;
					
					case TYPE_CHAR:
						data = (char)packet.get(offset);
						offset += 1;
						break;
						
					case TYPE_STRING:
						String str = readstring(offset);
						if (str == null) {
							throw new BufferUnderflowException();
						}
						data = str;
						offset += str.length() + 1;
						break;
						
					default:
						throw new UnsupportedDataTypeException("Return signature type '"+signature+"' unsupported");
				}
			} catch (BufferUnderflowException e) {
				return false;
			}
			
			packet.limit(packet.position());
			packet.position(offset);
			packet.compact();
			
			return true;
		}
		
		public void clear() { syscall = signature = null; data = null; }
		public char type() { return (char)packet.get(TYPE_OFFSET); }
		public String syscall() { return syscall; }
		public String signature() { return signature; }
		public Object data() { return data; }
	}
	
	
	private static final Pattern SIG_PATTERN = Pattern.compile("^([A-Za-z]?):([A-Za-z]*)$");
	
	private static final int CONSOLE_FRAMEING = 0xA5A5A5A5;
	
	private static final char TYPE_METHOD = 'M';
	private static final char TYPE_ERROR = 'E';
	private static final char TYPE_RETURN = 'R';
	
	private static final char TYPE_VOID = 'v';
	private static final char TYPE_BOOLEAN = 'b';
	private static final char TYPE_INTEGER = 'i';
	private static final char TYPE_DOUBLE = 'd';
	private static final char TYPE_CHAR = 'c';
	private static final char TYPE_STRING = 's';

	/* TODO - not now, but soon!
	private static final char TYPE_ARRAY_BOOLEAN = 'B';
	private static final char TYPE_ARRAY_INTEGER = 'I';
	private static final char TYPE_ARRAY_DOUBLE = 'D';
	private static final char TYPE_BUFFER = 'x';
	*/
	
	public static final int DEFAULT_PORT = 48000;
	
	private static final int SEND_BUFFER_SIZE = 128;
	
	private SocketChannel socket;
	private ByteBuffer sendbuf;
	
	private ReturnPacket reply;
	
	private Map<String, String> sigmap;
	
	public SyscallClient(InetSocketAddress sockaddress) throws IOException {
		socket = SocketChannel.open();
		socket.connect(sockaddress);
		
		sendbuf = ByteBuffer.allocate(SEND_BUFFER_SIZE).order(ByteOrder.LITTLE_ENDIAN);
		reply = new ReturnPacket();
		sigmap = Collections.synchronizedMap(new HashMap<String, String>());
		sigmap.put("syscall_signature", "s:s");
	}
	
	public SyscallClient(InetAddress address, int port) throws IOException {
		this(new InetSocketAddress(address, port));
	}
	
	public SyscallClient(InetAddress address) throws IOException {
		this(address, DEFAULT_PORT);
	}
	
	private void cache(String syscall) throws IOException, SyscallException {
		if (sigmap.containsKey(syscall)) {
			// Syscall already cached
			return;
		}
		
		String sig = (String)syscall("syscall_signature", syscall);
		if (sig.length() > 0) {
			sigmap.put(syscall, sig);
		}
	}
	
	public String signature(String syscall) throws IOException, SyscallException {
		cache(syscall);
		return sigmap.get(syscall);
	}
	
	public synchronized Object syscall(String syscall, Object ... args) throws IOException, SyscallException {
		String sig = signature(syscall);
		if (sig == null) {
			throw new SyscallNotFoundException("Syscall "+syscall+" doesn't exist");
		}
		
		Matcher matcher = SIG_PATTERN.matcher(sig);
		if (!matcher.matches()) {
			throw new UnsupportedDataTypeException("Bad signature: "+sig);
		}
		
		String retsig = matcher.group(1);
		if (retsig.length() == 0) {
			retsig = "v";
		}
		
		String argsig = matcher.group(2);
		
		sendbuf.clear();
		sendbuf.putInt(CONSOLE_FRAMEING);
		sendbuf.put((byte)TYPE_METHOD);
		
		sendbuf.put(syscall.getBytes(Charset.forName("US-ASCII")));
		sendbuf.put((byte)0);
		
		sendbuf.put(sig.getBytes(Charset.forName("US-ASCII")));
		sendbuf.put((byte)0);
		
		for (int i = 0; i < argsig.length(); i++) {
			try {
				char s = argsig.charAt(i);
				if (s == TYPE_VOID) {
					continue;
				}
				
				Object o = args[i];
				switch (s) {
					case TYPE_BOOLEAN:
						sendbuf.put((Boolean)o? (byte)1 : (byte)0);
						break;
						
					case TYPE_INTEGER:
						sendbuf.putInt((Integer)o);
						break;
						
					case TYPE_DOUBLE:
						sendbuf.putDouble((Double)o);
						break;
					
					case TYPE_CHAR:
						sendbuf.put((byte)((Character)o).charValue());
						break;
						
					case TYPE_STRING:
						sendbuf.put(((String)o).getBytes(Charset.forName("US-ASCII")));
						sendbuf.put((byte)0);
						break;
					
					default:
						throw new UnsupportedDataTypeException("Signature type '"+s+"' unsupported (Argument #"+i+")");
				}
				
			} catch (IndexOutOfBoundsException e) {
				throw new InvalidClassException("Argument "+i+" not given");
			} catch (ClassCastException e) {
				throw new InvalidClassException("Bad class given as argument "+i);
			}
		}
		
		sendbuf.flip();
		if (socket.write(sendbuf) != sendbuf.limit()) {
			throw new IOException("Could not send all data to server");
		}
		
		try {
			while (!reply.read(socket)) {}
			
			if (!syscall.equals(reply.syscall())) {
				throw new UnsupportedDataTypeException("Bad reply syscall name: "+reply.syscall());
			}
			
			if (!retsig.equals(reply.signature())) {
				throw new UnsupportedDataTypeException("Bad reply syscall sig: "+reply.signature());
			}
			
			switch (reply.type()) {
				case TYPE_RETURN:
					return reply.data();
					
				case TYPE_ERROR:
					throw new SyscallRemoteException((String)reply.data());
					
				default:
					throw new UnsupportedDataTypeException("Unknown reply type: "+reply.type());
			}
			
		} finally {
			reply.clear();
		}
	}
	
	public void close() throws IOException {
		socket.close();
	}
}
