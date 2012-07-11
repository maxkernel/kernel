package org.maxkernel.service.format;

public class Payload<T> {
	
	private final long timestamp;
	private final T payload;
	
	public Payload(long timestamp, T payload) {
		this.timestamp = timestamp;
		this.payload = payload;
	}
	
	public long timestamp() { return timestamp; }
	public T payload() { return payload; }
}
