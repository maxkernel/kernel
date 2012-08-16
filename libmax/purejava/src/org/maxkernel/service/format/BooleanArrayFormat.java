package org.maxkernel.service.format;

import java.util.concurrent.BlockingQueue;

import org.maxkernel.service.ServicePacket;

public class BooleanArrayFormat implements ServiceFormat<boolean[]> {
	private BlockingQueue<ServicePacket> queue;
	
	public BooleanArrayFormat(BlockingQueue<ServicePacket> queue) {
		this.queue = queue;
	}
	
	public FormattedServicePacket<boolean[]> dequeue() throws InterruptedException {
		ServicePacket p = queue.take();
		
		byte[] data = p.data();
		boolean[] a = new boolean[data.length];
		for (int i = 0; i < data.length; i++) {
			a[i] = data[i] > 0? true : false;
		}
		
		return new FormattedServicePacket<boolean[]>(p.timestamp(), a);
	}
	
	public void clear() {
		queue.clear();
	}
}
