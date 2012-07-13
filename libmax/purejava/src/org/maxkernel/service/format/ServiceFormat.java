package org.maxkernel.service.format;

import java.io.IOException;

public interface ServiceFormat<T> {
	public FormattedServicePacket<T> dequeue() throws IOException, InterruptedException;
	public void clear();
}
