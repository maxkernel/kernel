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

/**
 * This class provides remote syscall execution over TCP sockets.
 * 
 * In order for remote syscall execution to work, the Console module
 * needs <code>enable_network</code> option set (NOT the default). The easiest
 * way to enable this is to modify <code>max.conf</code> (usually located at
 * <code>/usr/lib/maxkernel/max.conf</code>). Append the line
 * <code>config("console", "enable_network", 1)</code> to the end of the file.
 * 
 * To execute syscalls, you must know the name of the syscall, and the signature
 * The signature follows the following pattern:
 * {@code <return type>:<argument 1 type><argument 2 type>...}
 * The types defined are all single character case-sensitive (no spaces). They are:
 *   - <code>v</code> - Void
 *   - <code>b</code> - Boolean
 *   - <code>i</code> - Integer
 *   - <code>d</code> - Double
 *   - <code>c</code> - Character
 *   - <code>s</code> - String
 *   .
 * 
 * The following signatures are valid:
 *   - <code>v:id</code> - (returns void, takes integer, double)
 *   - <code>i:i</code> - (returns integer, takes integer)
 *   - <code>d:v</code> - (returns double, takes no arguments) [the <code>v</code> argument type is optional]
 *   .
 * 
 * The following signatures are invalid:
 *   - <code>i:B</code> - (<code>B</code> is not a valid type. Remember, case-sensitive)
 *   - <code>q:q</code> - (<code>q</code> is not a valid type)
 *   - <code>v:4</code> - (<code>4</code> is not a valid type)
 *   - <code>v: i</code> - (There is a space in the signature)
 *   - <code>i:i:d</code> - (too many <code>:</code>'s)
 *   .
 * 
 * Once you know the name and signature of a syscall you want to execute, call the
 * {@link #syscall(String, Object...)} method. Make sure to feed the right argument
 * types to the method, or a {@link SyscallException} will be thrown. Here is an example
 * of executing syscall <code>some_syscall</code> with signature <code>d:is</code>
 * {@code
 * // Set the arguments
 * int arg1 = 42;
 * String arg2 = "Hello, MaxKernel!"
 * 
 * // Execute syscall some_syscall(d:is). Notice the Generics cast <Double> as the return type
 * double return_value = client.<Double>syscall("some_syscall", arg1, arg2);
 * 
 * }
 * 
 * Now, to put it all together.
 * Connecting to MaxKernel and executing syscalls is easy.
 * Simple example:
 * {@code
 * // Create the client and connect to MaxKernel
 * SyscallClient client = new SyscallClient(InetAddress.getByName("192.168.1.102")); // Replace with robot IP
 * 
 * // Check to see if kernel_installed(i:v) syscall exists by using the syscall_exists(b:ss) syscall
 * // (We know both are always provided by default, but this is an example...)
 * boolean exists = client.<Boolean>syscall("syscall_exists", "kernel_installed", "i:v");
 * 
 * // Now call kernel_installed
 * if (exists) {
 *     long timestamp = client.<Integer>syscall("kernel_installed");
 *     
 *     Date install_date = new Date(timestamp * 1000); // Timestamp is in seconds from epoc
 *     System.out.println(install_date);
 * } else {
 *     System.err.println("Oh no! kernel_installed doesn't exist!");
 * }
 * 
 * // Close the client
 * client.close();
 * 
 * }
 * 
 * This example will list the names, signatures, and descriptions of each syscall available
 * {@code
 * // Create the client and connect to MaxKernel
 * SyscallClient client = new SyscallClient(InetAddress.getByName("192.168.1.102")); // Replace with robot IP
 * 
 * // Get a syscalls iterator
 * int itr = client.<Integer>syscall("syscalls_itr");
 * 
 * // Iterate over all the available syscalls (empty string represents end)
 * String name = client.<String>syscall("syscalls_next", itr);
 * while (name.length() != 0) {
 * 
 *     // Get the signature of the syscall
 *     String signature = client.<String>syscall("syscall_signature", name);
 *     
 *     // Get the description of the syscall
 *     String description = client.<String>syscall("syscall_info", name);
 *     
 *     System.out.println(String.format("%s(%s) - %s", name, signature, description));
 *     
 *     // Iterate to the next syscall 
 *     name = client.<String>syscall("syscalls_next", itr);
 * }
 * 
 * // Make sure to free the iterator when done
 * client.<Void>syscall("itr_free", itr);
 * 
 * // Close the client when you're done
 * client.close();
 * 
 * } 
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class SyscallClient {
	
	/**
	 * Internal packet parsing class
	 */
	private static final class ReturnPacket {
		private static final int SIZE = 256;
		
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
	
	/**
	 * The default TCP port.
	 */
	public static final int DEFAULT_PORT = 48000;
	
	private static final int SEND_BUFFER_SIZE = 128;
	
	private SocketChannel socket;
	private ByteBuffer sendbuf;
	
	private ReturnPacket reply;
	
	private Map<String, String> sigmap;
	
	/**
	 * Constructs a new SyscallClient and connects to the given host address over TCP.
	 * @param sockaddress The remote address to connect to.
	 * @throws IOException If there was a problem connecting.
	 */
	public SyscallClient(InetSocketAddress sockaddress) throws IOException {
		socket = SocketChannel.open();
		socket.connect(sockaddress);
		
		sendbuf = ByteBuffer.allocate(SEND_BUFFER_SIZE).order(ByteOrder.LITTLE_ENDIAN);
		reply = new ReturnPacket();
		sigmap = Collections.synchronizedMap(new HashMap<String, String>());
		clearCache();
	}
	
	/**
	 * Constructs a new SyscallClient and connects to the given address and port over TCP.
	 * @param address The remote address to connect to.
	 * @param port The TCP port to use.
	 * @throws IOException If there was a problem connecting.
	 */
	public SyscallClient(InetAddress address, int port) throws IOException {
		this(new InetSocketAddress(address, port));
	}
	
	/**
	 * Constructs a new SyscallClient and connects to the given address over TCP using the default port.
	 * @param address The remote address to connect to.
	 * @throws IOException If there was a problem connecting.
	 * @see #DEFAULT_PORT
	 */
	public SyscallClient(InetAddress address) throws IOException {
		this(address, DEFAULT_PORT);
	}
	
	private void cache(String syscall) throws IOException, SyscallException {
		if (sigmap.containsKey(syscall)) {
			// Syscall already cached
			return;
		}
		
		String sig = this.<String>syscall("syscall_signature", syscall);
		if (sig.length() > 0) {
			sigmap.put(syscall, sig);
		}
	}
	
	/**
	 * Obtains the signature of the given syscall and returns it. If the signature does
	 * not exist in cache, a remote request is made. 
	 * @param syscall The name of the syscall
	 * @return The signature of the syscall
	 * @throws IOException If there is an IO problem requesting the the signature remotely 
	 * @throws SyscallException If there is a problem requesting the the signature remotely
	 */
	public String getSignature(String syscall) throws IOException, SyscallException {
		cache(syscall);
		return sigmap.get(syscall);
	}
	
	/**
	 * Clears the syscall to signature mapping cache.
	 */
	public void clearCache() {
		sigmap.clear();
		sigmap.put("syscall_signature", "s:s");
	}
	
	/**
	 * Executes a remote syscall. For a brief description and tutorial, see the overall documentation
	 * of this class.
	 * @param syscall The name of the syscall.
	 * @param args The arguments of the syscall.
	 * @return The return value of the syscall.
	 * @throws IOException If there was a problem communicating with the host.
	 * @throws SyscallException If there was a problem executing the syscall.
	 */
	@SuppressWarnings("unchecked")
	public synchronized <T> T syscall(String syscall, Object ... args) throws IOException, SyscallException {
		String sig = getSignature(syscall);
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
					return (T)reply.data();
					
				case TYPE_ERROR:
					throw new SyscallRemoteException((String)reply.data());
					
				default:
					throw new UnsupportedDataTypeException("Unknown reply type: "+reply.type());
			}
			
		} finally {
			reply.clear();
		}
	}
	
	/**
	 * Closes the TCP communication socket, The class instance can no longer be used to
	 * execute syscalls.  
	 * @throws IOException If there was a problem closing the TCP socket.
	 */
	public void close() throws IOException {
		socket.close();
	}
}
