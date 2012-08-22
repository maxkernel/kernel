package org.maxkernel.service.queue;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.concurrent.BlockingQueue;

import org.maxkernel.service.ServicePacket;

/**
 * This class wraps a BlockingQueue and converts a <code>byte[]</code> {@link ServicePacket}
 * into a <code>int[]</code>.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class IntegerArrayServiceQueue extends ServiceQueue<int[]> {

	/**
	 * Creates a new blocking service queue with the given backing queue.
	 * @param queue The backing queue.
	 */
	public IntegerArrayServiceQueue(BlockingQueue<ServicePacket<byte[]>> queue) {
		super(queue);
	}

	@Override
	protected int[] transmute(byte[] data) {
		IntBuffer buffer = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
		int[] array = new int[buffer.capacity()];
		buffer.get(array);
		return array;
	}
}
