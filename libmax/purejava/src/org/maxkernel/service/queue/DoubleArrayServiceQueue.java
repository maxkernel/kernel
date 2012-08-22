package org.maxkernel.service.queue;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.DoubleBuffer;
import java.util.concurrent.BlockingQueue;

import org.maxkernel.service.ServicePacket;

/**
 * This class wraps a BlockingQueue and converts a <code>byte[]</code> {@link ServicePacket}
 * into a <code>double[]</code>.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class DoubleArrayServiceQueue extends ServiceQueue<double[]> {
	
	/**
	 * Creates a new blocking service queue with the given backing queue.
	 * @param queue The backing queue.
	 */
	public DoubleArrayServiceQueue(BlockingQueue<ServicePacket<byte[]>> queue) {
		super(queue);
	}
	
	@Override
	protected double[] transmute(byte[] data) {
		DoubleBuffer buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).asDoubleBuffer();
		double[] array = new double[buffer.capacity()];
		buffer.get(array);
		return  array;
	}
}
