package org.maxkernel.service.streams;

import java.io.IOException;
import java.io.SyncFailedException;
import java.nio.channels.Selector;
import java.util.Map;

import org.maxkernel.service.Service;
import org.maxkernel.service.ServiceClient;
import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.queue.ServiceQueue;

/**
 * Base interface for Stream objects.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public interface Stream {
	
	/**
	 * Represents the mode of the stream.
	 * 
	 * @author Andrew Klofas
	 * @version 1.0
	 */
	public enum Mode {
		/**
		 * Service listing and subscribe/unsubscribe methods are available.
		 */
		UNLOCKED,
		
		/**
		 * Service is currently streaming and meta-services are no longer supported.
		 */
		LOCKED,
	}
	
	/**
	 * Service protocol disconnect opcode. (private use)
	 */
	public static final byte GOODBYE		= 0x00;
	
	
	//public static final byte AUTH			= 0x01;
	
	/**
	 * Service protocol heartbeat opcode. (private use)
	 */
	public static final byte HEARTBEAT	= 0x02;
	
	/**
	 * Service protocol subscribe opcode. (private use)
	 */
	public static final byte SUBSCRIBE	= 0x03;
	
	/**
	 * Service protocol unsubscribe opcode. (private use)
	 */
	public static final byte UNSUBSCRIBE	= 0x04;
	
	/**
	 * Service protocol begin streaming opcode. (private use)
	 */
	public static final byte BEGIN		= 0x05;
	
	/**
	 * Service protocol data payload opcode. (private use)
	 */
	public static final byte DATA			= 0x06;
	
	/**
	 * Service protocol xml service listing opcode. (private use)
	 */
	public static final byte LISTXML		= 0x11;
	
	
	/**
	 * @return The state of this stream.
	 * @see Mode
	 */
	public Mode getMode();
	
	/**
	 * Queries for all available services and returns a map of
	 * service names -> service objects.
	 * @return The map of all available services.
	 * @throws IOException If there was a problem reading the services
	 */
	public Map<String, Service> getServices() throws IOException;
	
	/**
	 * Subscribes to the given service but does not begin streaming yet.
	 * To start streaming, register this stream and an associated blocking queue
	 * (or {@link ServiceQueue}) with a ServiceClient using the
	 * {@link ServiceClient#begin(Stream, java.util.concurrent.BlockingQueue)} or
	 * {@link ServiceClient#begin(Stream, ServiceQueue)} methods.
	 * 
	 * Calling this method assumes that the stream is unlocked (see {@link #getMode()}).
	 * If the stream is locked, a {@link SyncFailedException} is thrown.
	 * 
	 * @param service The service to subscribe to
	 * @throws IOException If the is an error in the communication
	 */
	public void subscribe(Service service) throws IOException;
	
	/**
	 * Unsubscribes from the previously subscribed service. Once the stream is locked,
	 * it cannot be unsubscribed and must be {@link #close()} and recreated. With that
	 * in mind, this method is mostly useless, but it's included here for symmetry.
	 * 
	 * Calling this method assumes that the stream is unlocked (see {@link #getMode()}).
	 * If the stream is locked, a {@link SyncFailedException} is thrown.
	 * 
	 * @throws IOException If the is an error in the communication
	 */
	public void unsubscribe() throws IOException;
	
	/**
	 * Used internally by {@link ServiceClient}. Called when locking into a
	 * streaming state.
	 * @param selector The selector to register with.
	 * @throws IOException If there was an errors registering with the given selector
	 */
	public void begin(Selector selector) throws IOException;
	
	/**
	 * Used internally by {@link ServiceClient}. Called periodically to send
	 * heartbeats remotely.
	 * @throws IOException If there was an error sending the heartbeat.
	 */
	public void heartbeat() throws IOException;
	
	/**
	 * Used internally by {@link ServiceClient}. Called periodically to determine
	 * disconnects and heartbeat errors.
	 * @return True if stream is okay
	 */
	public boolean checkIO();
	
	/**
	 * Used internally by {@link ServiceClient}. Called when new IO data is available
	 * to be processed.
	 * @return A new un-transmuted service packet, or <code>null</code>
	 * @throws IOException If there was a problem reading/writing IO.
	 */
	public ServicePacket<byte[]> handleIO() throws IOException;
	
	/**
	 * Closes the current stream. This method is intended to mostly be used
	 * internally by {@link ServiceClient}. You should probably be using
	 * {@link ServiceClient#close()} most of the time. Only use this method
	 * if you know what you're doing.
	 * @throws IOException If there was a problem closing the stream.
	 */
	public void close() throws IOException;
}
