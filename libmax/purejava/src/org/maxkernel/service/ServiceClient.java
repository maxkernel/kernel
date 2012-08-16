package org.maxkernel.service;

import java.io.IOException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.maxkernel.service.streams.Stream;

/**
 * A container object that can handle multiple {@link Stream} instances. This class spawns a single
 * thread that provides to {@link Stream} instances registered with it:
 *   - Heartbeating
 *   - Timeout monitoring
 *   - Disconnection callbacks
 *   - Queueing {@link ServicePacket}s in a given BlockingQueue
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class ServiceClient {
	private static final Logger LOG = Logger.getLogger(ServiceClient.class.getCanonicalName());
	
	private static int HEARTBEAT_PERIOD		= 500;
	private static int CHECK_PERIOD			= 500;
	
	private static int SELECTOR_TIMEOUT		= 100;
	
	private Set<Stream> streams;
	private Map<Stream, BlockingQueue<ServicePacket>> packets;
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
		packets = Collections.synchronizedMap(new HashMap<Stream, BlockingQueue<ServicePacket>>());
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
							
							ex.printStackTrace();
							
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
						if (!stream.check()) {
							System.out.println("CHECK FAIL");
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
					if (keys.size() == 0) {
						continue;
					}
					
					for (SelectionKey key : keys) {
						Stream stream = (Stream)key.attachment();
						
						try {
							ServicePacket packet = stream.handle();
							if (packet != null) {
								BlockingQueue<ServicePacket> queue = packets.get(packet.stream());
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
			exc.printStackTrace();
			
		} catch (Exception exc) {
			LOG.log(Level.WARNING, "Exception thrown while trying to close stream", exc);
		}
		
		System.out.println("DISCONNECT!");
		// TODO - handle disconnected listener
	}
	
	public void begin(Stream stream, BlockingQueue<ServicePacket> queue) throws IOException {
		if (stream == null || queue == null) {
			throw new IllegalArgumentException("Bad arguments");
		}
		
		synchronized (streams) {
			if (streams.contains(stream)) {
				throw new IllegalArgumentException("Stream has already been added!");
			}
			
			streams.add(stream);
		}
		
		packets.put(stream, queue);
		stream.begin(selector);
		selector.wakeup();
	}
	
	public void close() throws IOException {
		executor.shutdown();
		closeflag = true;
		selector.close();
		
		for (Stream stream : streams.toArray(new Stream[streams.size()])) {
			closestream(stream);
		}
	}
}
