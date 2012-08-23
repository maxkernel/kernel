package org.maxkernel.service;

import java.util.EventListener;
import java.util.EventObject;

import org.maxkernel.service.streams.Stream;

/**
 * The listener interface for receiving DisconnectEvents when a Stream is disconnected
 * from the service host (either manually or automatically).
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public interface DisconnectListener extends EventListener {
	
	/**
	 * An event object that contains information about a specific disconnection.
	 * 
	 * @author Andrew Klofas
	 * @version 1.0
	 */
	public static class DisconnectEvent extends EventObject {
		private static final long serialVersionUID = -836206134387895949L;
		
		private long timestamp;
		private Service service;
		
		/**
		 * Constructs a DisconnectEvent object.
		 * @param source The Stream that was disconnected.
		 */
		public DisconnectEvent(Stream source) {
			super(source);
			timestamp = System.currentTimeMillis();
			service = source.getService();
		}
		
		/**
		 * @return The Stream object that was disconnected.
		 */
		public Stream getStream() {
			return (Stream)getSource();
		}
		
		/**
		 * @return The Service that the stream was subscribed to, or <code>null</code> if unsubscribed.
		 */
		public Service getService() {
			return service;
		}
		
		/**
		 * @return A millisecond timestamp when the event was detected.
		 */
		public long getTimestamp() {
			return timestamp;
		}
	}
	
	/**
	 * Invoked when a Stream disconnection occurs.
	 * @param e The event information.
	 */
	public void streamDisconnected(DisconnectEvent e);
}
