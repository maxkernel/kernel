package org.maxkernel.service;

import org.maxkernel.service.ServicePacket;
import org.maxkernel.service.queue.ServiceQueue;
import org.maxkernel.service.streams.Stream;

/**
 * An instance of this class represents one sample point or datum of information
 * in the continuous stream from a service. When you start streaming data from
 * a service, service packets are what's sent at specific intervals. (The interval
 * period is defined by the service).
 * 
 * Initially, the data is represented as a raw binary blob (byte array). To format the
 * data into a more usable form, use {@link ServiceQueue} class and friends when
 * registering {@link Stream}s with a {@link ServiceClient} 
 * 
 * @author Andrew Klofas
 * @version 1.0
 * 
 * @param <T> The class of the formatted object. (Example <code>BufferedImage</code>
 * or <code>double[]</code>)
 * @see ServiceQueue
 */
public class ServicePacket<T> {
	private final Service service;
	private final Stream stream;
	
	private final long timestamp;
	private final T data;
	
	/**
	 * Creates a new service packet associated with the given service and
	 * stream containing the given data and data timestamp (in microseconds).
	 * @param service The originating service that created this data.
	 * @param stream Which stream was used to transmit the data.
	 * @param timestamp Microsecond timestamp tagged on the remote end when
	 * the data was created.
	 * @param data The payload data of the packet
	 */
	public ServicePacket(Service service, Stream stream, long timestamp, T data) {
		this.service = service;
		this.stream = stream;
		this.timestamp = timestamp;
		this.data = data;
	}
	
	/**
	 * @return The service associated with this packet
	 */
	public Service getService() {
		return this.service;
	}
	
	/**
	 * @return The stream associated with this packet
	 */
	public Stream getStream() {
		return this.stream;
	}
	
	/**
	 * @return The microsecond timestamp associated with this data.
	 * (The timestamp was tagged on the remote side) 
	 */
	public long getTimestamp() {
		return timestamp;
	}
	
	/**
	 * @return The service data
	 * @see ServicePacket
	 */
	public T getData() {
		return data;
	}
}
