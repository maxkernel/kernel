package org.maxkernel.service.format;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.DoubleBuffer;
import java.util.concurrent.BlockingQueue;

import org.maxkernel.service.ServicePacket;

public class DoubleArrayFormat implements ServiceFormat<double[]> {

	private BlockingQueue<ServicePacket> queue;
	
	public DoubleArrayFormat(BlockingQueue<ServicePacket> queue) {
		this.queue = queue;
	}
	
	public FormattedServicePacket<double[]> dequeue() throws InterruptedException {
		ServicePacket p = queue.take();
		DoubleBuffer b = ByteBuffer.wrap(p.data()).order(ByteOrder.LITTLE_ENDIAN).asDoubleBuffer();
		
		double[] a = new double[b.capacity()];
		b.get(a);
		
		return new FormattedServicePacket<double[]>(p.timestamp(), a);
	}
}
