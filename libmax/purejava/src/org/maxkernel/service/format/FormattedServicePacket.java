package org.maxkernel.service.format;

public class FormattedServicePacket<T> {
	
	private final long timestamp;
	private final T data;
	
	public FormattedServicePacket(long timestamp, T data) {
		this.timestamp = timestamp;
		this.data = data;
	}
	
	public long timestamp() { return timestamp; }
	public T data() { return data; }
}
