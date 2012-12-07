package org.maxkernel.service;

import java.io.IOException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.maxkernel.service.DisconnectListener.DisconnectEvent;
import org.maxkernel.service.queue.ServiceQueue;
import org.maxkernel.service.streams.Stream;
import org.maxkernel.service.streams.TCPStream;
import org.maxkernel.service.streams.UDPStream;

/**
 * This class is used to monitor and handle the service streams registered with the robot. This class
 * spawns two threads when created. The first thread handles the IO of the registered streams
 * (see {@link #begin(Stream, BlockingQueue)}. The second thread provides heartbeating, timeout
 * monitoring, disconnection callbacks, and queueing {@link ServicePacket}s.
 * 
 * On each MaxKernel robot with services enabled (the default configuration), you can browse and
 * stream configured services. Services are read-only data packets sent from the robot at fixed or
 * periodic intervals. They can contain data in any format, so this library also contains helper
 * classes for conversion into standard formats. Examples of services include:
 *   - GPS coordinates
 *   - Wireless network strength
 *   - Video frames
 *   - Commanded motor moves
 *   - Logs
 *   - etc.
 *   .
 * 
 * All service names, update frequencies, data format, etc are configured in MaxKernel by
 * configuration scripts during runtime. As such, a ServiceClient can only detect and subscribe to
 * existing services and can in no way modify or interact with MaxKernel operations (see
 * {@link SyscallClient} if you want to interact). This means that security is less of an issue
 * and many different applications can monitor the robot without needing to coordinate.
 * 
 * The physical streaming medium that the service data is transmitted over is left for the
 * client to decide. Any service can be streamed over any medium (provided it's
 * enabled in MaxKernel). For example, you can stream GPS coordinates over a TCP socket if you
 * want reliable delivery of that data, but with possible delays and retries. Or the same data
 * can be transmitted over a UDP socket with bounded latency but possible data loss. You can
 * even use both of these streaming mediums at the same time if you have a need to do immediate
 * GPS processing as well as reliable GPS logging.
 * 
 * Here is an example of how to list the available services over a TCP link:
 * {@code
 * // Connect over a TCP socket using the default port
 * Stream tcp_stream = new TCPStream(InetAddress.getByName("192.168.1.102")); // Replace with robot IP
 * 
 * // Get the available services (service name -> service object map)
 * Map<String, Service> services = tcp_stream.listServices();
 * 
 * // Remember to close the stream when done
 * // Note - you'll probably never be closing the stream directly like this,
 * //    you'll want to use ServiceClient.close() instead, explained later
 * tcp_stream.close();
 * 
 * // Iterate over services map and print
 * for (Service s : services.values()) {
 *     String name = s.getName();
 *     String format = s.getFormat();
 *     String description = s.getDescription();
 *     
 *     System.out.println(String.format("%s - (format = %s) %s", name, format, description));
 * }
 * 
 * }
 * 
 * You can accomplish the exact same thing over a UDP socket by using {@link UDPStream} instead
 * of {@link TCPStream}.
 * 
 * At some point, you want to actually to subscribe and stream the service. Here is an example
 * of doing that.
 * {@code
 * // Create the service client
 * ServiceClient client = new ServiceClient();
 * 
 * // Create our TCPStream as before
 * Stream tcp_stream = new TCPStream(InetAddress.getByName("192.168.1.102")); // Replace with robot IP
 * 
 * // Get the service named 'gps'
 * Map<String, Service> services = tcp_stream.listServices();
 * Service gps_service = services.get("gps");  // TODO - check for null
 * 
 * // Subscribe to the 'gps' service
 * tcp_stream.subscribe(gps_service);
 * 
 * // Create our blocking queue that the raw gps data will go into
 * BlockingQueue<ServicePacket<byte[]>> gps_raw_queue = new LinkedBlockingQueue<>();
 * 
 * // The blocking queue will contain ServicePackets of type byte[] (the default format).
 * // The ServiceClient is unable to automatically convert the service data.
 * // In order to convert the data, we must know before hand the format of the data
 * // In this example, however, we know that the 'gps' service outputs a double[] (double array)
 * // So, we will make a new ServiceQueue that wraps our gps_raw_queue and outputs a double[]
 * ServiceQueue<double[]> gps_queue = ServiceQueue.make(double[].class, gps_raw_queue); // TODO - check for exceptions
 * 
 * // Start the streaming. The service client will put ServicePackets received
 * // by tcp_stream into the gps_packet queue
 * client.begin(tcp_stream, gps_queue);
 * 
 * // Loop over 10 packets
 * for (int i=0; i<10; i++) {
 *     // Taking from the queue will automatically convert to double[]
 *     ServicePacket<double[]> gps_packet = gps_queue.take();
 *     
 *     long timestamp_us = gps_packet.getTimestamp();  // microsecond timestamp
 *     double[] data = gps_packet.getData();
 *     
 *     Date sample_time = new Date(timestamp_us / 1000);
 *     System.out.println(String.format("Lat: %f, Long: %f, Time: %s", data[0], data[1], sample_time));
 * }
 * 
 * // Close the ServiceClient (will close all registered streams too)
 * client.close();
 * 
 * }
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class ServiceClient {
	private static final Logger LOG = Logger.getLogger(ServiceClient.class.getCanonicalName());
	
	private static int HEARTBEAT_PERIOD		= 500;
	private static int CHECK_PERIOD			= 500;
	
	private static int SELECTOR_TIMEOUT		= 50;
	
	private Set<Stream> streams;
	private Map<Stream, BlockingQueue<ServicePacket<byte[]>>> packets;
	private List<DisconnectListener> listeners;
	private Queue<Stream> addqueue;
	private Selector selector;
	
	private ScheduledThreadPoolExecutor executor;
	private volatile boolean closeflag;
	
	/**
	 * Creates a new ServiceClient. This will spawn a new daemon thread that handles
	 * the IO of each registered {@link Stream} asynchronously. The thread can be
	 * canceled by calling {@link #close()}.
	 * @throws IOException
	 * @see #close()
	 */
	public ServiceClient() throws IOException {
		streams = new HashSet<Stream>();
		packets = Collections.synchronizedMap(new HashMap<Stream, BlockingQueue<ServicePacket<byte[]>>>());
		listeners = new ArrayList<>();
		addqueue = new ConcurrentLinkedQueue<Stream>();
		selector = Selector.open();
		closeflag = false;
		
		executor = new ScheduledThreadPoolExecutor(1, new ThreadFactory() {
			@Override
			public Thread newThread(Runnable r) {
				Thread t = new Thread(r);
				t.setDaemon(true);
				return t;
			}
		});
		
		// Set up the heartbeat task
		executor.scheduleAtFixedRate(new Runnable() {
			@Override
			public void run() {
				synchronized (streams) {
					for (Stream stream : streams) {
						try {
							stream.heartbeat();
							
						} catch (IOException ex) {
							closestream(stream);
							
						} catch (Exception ex) {
							LOG.log(Level.WARNING, "Exception thrown while heartbeating stream", ex);
							closestream(stream);
							
						}
					}
				}
			}
		}, 0, HEARTBEAT_PERIOD, TimeUnit.MILLISECONDS);
		
		// Set up the check task
		executor.scheduleAtFixedRate(new Runnable() {
			@Override
			public void run() {
				synchronized (streams) {
					for (Stream stream : streams) {
						if (!stream.checkIO()) {
							closestream(stream);
							
						}
					}
				}
			}
		}, 0, CHECK_PERIOD, TimeUnit.MILLISECONDS);
		
		// Set up the selector task
		Thread t = new Thread(new Runnable() {
			@Override
			public void run() {
				while (true) {
					try {
						selector.select(SELECTOR_TIMEOUT);
					} catch (IOException ex) {
						LOG.log(Level.WARNING, "Exception thrown during service select() operation", ex);
						continue;
					}
					
					if (closeflag) {
						return;
					}
					
					Set<SelectionKey> keys = selector.selectedKeys();
					if (keys.size() > 0) {
						for (SelectionKey key : keys) {
							Stream stream = (Stream)key.attachment();
							
							try {
								ServicePacket<byte[]> packet = stream.handleIO();
								if (packet != null) {
									BlockingQueue<ServicePacket<byte[]>> queue = packets.get(packet.getStream());
									if (queue != null) {
										queue.offer(packet);
									}
								}
								
							} catch (IOException ex) {
								ex.printStackTrace();
								
								key.cancel();
								closestream(stream);
								
							} catch (Exception ex) {
								LOG.log(Level.WARNING, "Exception thrown while handling stream io", ex);
								
							}
						}
					}
					
					while (addqueue.size() > 0) {
						Stream newstream = addqueue.remove();
						
						try {
							newstream.begin(selector);
							
						} catch (IOException ex) {
							LOG.log(Level.WARNING, "Exception thrown while adding stream to selector", ex);
							closestream(newstream);
						}
					}
				}
			}
		});
		t.setDaemon(true);
		t.start();
	}
	
	private void closestream(Stream stream) {
		synchronized (streams) {
			if (!streams.contains(stream)) {
				return;
			}
			
			streams.remove(stream);
		}
		
		try {
			stream.close();
		} catch (IOException exc) {
			// Do nothing, we are already trying to close the stream
			
		} catch (Exception exc) {
			LOG.log(Level.WARNING, "Exception thrown while trying to close stream", exc);
		}
		
		fireDisconnected(new DisconnectEvent(stream));
	}
	
	/**
	 * Adds a DisconnectListener to be called when a stream is disconnected (manually or automatically).
	 * @param listener The listener to call.
	 */
	public void addDisconnectListener(DisconnectListener listener) {
		listeners.add(listener);
	}
	
	/**
	 * Removes the first occurrence of the given DisconnectListener from the list.
	 * @param listener The listener to remove.
	 */
	public void removeDisconnectListener(DisconnectListener listener) {
		listeners.remove(listener);
	}
	
	/**
	 * @return All the registered DisconnectListeners
	 */
	public DisconnectListener[] getDisconnectListeners() {
		return listeners.toArray(new DisconnectListener[0]);
	}
	
	/**
	 * Dispatches the given event to all DisconnectListeners
	 * @param e The event to dispatch
	 */
	protected void fireDisconnected(DisconnectEvent e) {
		for (DisconnectListener l : listeners) {
			try {
				l.streamDisconnected(e);
				
			} catch (Exception ex) {
				LOG.log(Level.WARNING, "Exception thrown while dispatching DisconnectListener", ex);
			}
		}
	}
	
	/**
	 * Registers the given stream and begins streaming {@link ServicePacket}s into the given queue.
	 * Prior to calling this method, the stream needs to be connected to the remote host and subscribed
	 * to a service.
	 * @param stream Stream object to register with this client.
	 * @param queue BlockingQueue to add service packets to.
	 * @throws IOException if there was a problem starting the stream.
	 * @throws IllegalArgumentException if one of the arguments are <code>null</code> or
	 * the stream has already been registered with this client.
	 * @throws IllegalStateException if attempting to begin streaming while in the process
	 * of closing this serviceclient
	 */
	public void begin(Stream stream, BlockingQueue<ServicePacket<byte[]>> queue) throws IOException, IllegalArgumentException, IllegalStateException {
		if (stream == null || queue == null) {
			throw new IllegalArgumentException("Bad arguments");
		}
		
		if (closeflag) {
			throw new IllegalStateException("In the process of closing this serviceclient!");
		}
		
		synchronized (streams) {
			if (streams.contains(stream)) {
				throw new IllegalArgumentException("Stream has already been added!");
			}
			
			streams.add(stream);
		}
		
		packets.put(stream, queue);
		addqueue.add(stream);
		selector.wakeup();
	}
	
	/**
	 * Registers the given stream and begins streaming {@link ServicePacket}s into the given queue.
	 * This method adheres to the same contracts as {@link #begin(Stream, BlockingQueue)}, except it takes
	 * a {@link ServiceQueue} instead of a BlockingQueue
	 * @param stream Stream object to register with this client.
	 * @param queue {@link ServiceQueue} to add service packets to.
	 * @throws IOException if there was a problem starting the stream.
	 * @throws IllegalArgumentException if one of the arguments are <code>null</code> or
	 * the stream has already been registered with this client.
	 * @throws IllegalStateException if attempting to begin streaming while in the process
	 * of closing this serviceclient
	 * @see #begin(Stream, BlockingQueue)
	 */
	public void begin(Stream stream, ServiceQueue<?> queue) throws IOException, IllegalArgumentException, IllegalStateException {
		if (stream == null || queue == null) {
			throw new IllegalArgumentException("Bad arguments");
		}
		
		begin(stream, queue.getBackingQueue());
	}
	
	/**
	 * Cancels the two threads and closes all registered streams. This will disconnect all
	 * registered streams, calling the disconnect callback of each stream
	 * @throws IOException if there was a problem closing the selector
	 */
	public void close() throws IOException {
		executor.shutdown();
		closeflag = true;
		
		for (Stream stream : streams.toArray(new Stream[streams.size()])) {
			closestream(stream);
		}
		
		selector.close();
	}
}
