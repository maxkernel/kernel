package org.maxkernel.service;

import org.maxkernel.service.streams.Stream;

public class ServicePacket {
	private Service service;
	private Stream stream;
	
	private long timestamp;
	private byte[] data;
	
	public ServicePacket(Service service, Stream stream, long timestamp, byte[] data) {
		this.service = service;
		this.stream = stream;
		this.timestamp = timestamp;
		this.data = data;
	}
	
	public Service service() { return this.service; }
	public Stream stream() { return this.stream; }
	public long timestamp() { return this.timestamp; }
	public byte[] data() { return this.data; }
}
