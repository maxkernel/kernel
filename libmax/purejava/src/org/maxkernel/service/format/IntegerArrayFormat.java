package org.maxkernel.service.format;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.concurrent.BlockingQueue;

import org.maxkernel.service.ServicePacket;

public class IntegerArrayFormat implements ServiceFormat<int[]> {

	private BlockingQueue<ServicePacket> queue;
	
	public IntegerArrayFormat(BlockingQueue<ServicePacket> queue) {
		this.queue = queue;
	}
	
	public FormattedServicePacket<int[]> dequeue() throws InterruptedException {
		ServicePacket p = queue.take();
		IntBuffer b = ByteBuffer.wrap(p.data()).order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
		
		int[] a = new int[b.capacity()];
		b.get(a);
		
		return new FormattedServicePacket<int[]>(p.timestamp(), a);
	}
	
	public void clear() {
		queue.clear();
	}
}
