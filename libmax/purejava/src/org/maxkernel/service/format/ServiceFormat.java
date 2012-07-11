package org.maxkernel.service.format;

import java.io.IOException;

public interface ServiceFormat<T> {
	public Payload<T> dequeue() throws IOException, InterruptedException;
}
