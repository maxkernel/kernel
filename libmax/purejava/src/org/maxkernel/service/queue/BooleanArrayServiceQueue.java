package org.maxkernel.service.queue;

import java.util.concurrent.BlockingQueue;

import org.maxkernel.service.ServicePacket;

/**
 * This class wraps a BlockingQueue and converts a <code>byte[]</code> {@link ServicePacket}
 * into a <code>boolean[]</code>.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class BooleanArrayServiceQueue extends ServiceQueue<boolean[]> {

	/**
	 * Creates a new blocking service queue with the given backing queue.
	 * @param queue The backing queue.
	 */
	public BooleanArrayServiceQueue(BlockingQueue<ServicePacket<byte[]>> queue) {
		super(queue);
	}

	@Override
	protected boolean[] transmute(byte[] data) {
		boolean[] array = new boolean[data.length];
		for (int i = 0; i < data.length; i++) {
			array[i] = data[i] > 0? true : false;
		}
		
		return array;
	}
}
