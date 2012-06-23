package org.maxkernel.service;

import java.io.IOException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.maxkernel.service.streams.Stream;

public class ServiceClient {
	private static final Logger LOG = Logger.getLogger(ServiceClient.class.getCanonicalName());
	
	private static int PACKET_QUEUE_LIMIT	= 25;
	private static int HEARTBEAT_PERIOD		= 500;
	private static int CHECK_PERIOD			= 500;
	
	private static int SELECTOR_TIMEOUT		= 200;
	
	private Set<Stream> streams;
	private BlockingQueue<ServicePacket> packets;
	private Selector selector;
	
	public ServiceClient() throws IOException {
		streams = new HashSet<Stream>();
		packets = new ArrayBlockingQueue<ServicePacket>(PACKET_QUEUE_LIMIT);
		selector = Selector.open();
		
		ScheduledThreadPoolExecutor e = new ScheduledThreadPoolExecutor(1, new ThreadFactory() {
			@Override
			public Thread newThread(Runnable r) {
				Thread t = new Thread(r);
				t.setDaemon(true);
				return t;
			}
		});
		
		// Set up the heartbeat task
		e.scheduleAtFixedRate(new Runnable() {
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
		e.scheduleAtFixedRate(new Runnable() {
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
					
					Set<SelectionKey> keys = selector.selectedKeys();
					if (keys.size() == 0) {
						continue;
					}
					
					for (SelectionKey key : keys) {
						Stream stream = (Stream)key.attachment();
						
						try {
							ServicePacket packet = stream.handle();
							if (packet != null) {
								if (packets.remainingCapacity() == 0) {
									packets.remove();
								}
								
								packets.offer(packet);
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
		} catch (Exception exc) {
			LOG.log(Level.WARNING, "Exception thrown while trying to close stream", exc);
		}
		
		System.out.println("DISCONNECT!");
		// TODO - handle disconnected listener
	}
	
	public void start(Stream stream) throws IOException {
		synchronized (streams) {
			if (streams.contains(stream)) {
				throw new IllegalArgumentException("Stream has already been added!");
			}
			
			streams.add(stream);
		}
		
		stream.begin(selector);
	}
	
	public ServicePacket dequeue() throws InterruptedException {
		return packets.take();
	}
}
