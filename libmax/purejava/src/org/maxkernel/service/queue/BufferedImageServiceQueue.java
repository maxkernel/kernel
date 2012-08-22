package org.maxkernel.service.queue;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.util.concurrent.BlockingQueue;

import javax.imageio.ImageIO;

import org.maxkernel.service.ServicePacket;

/**
 * This class wraps a BlockingQueue and converts a <code>byte[]</code> {@link ServicePacket}
 * into a <code>BufferedImage</code>.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class BufferedImageServiceQueue extends ServiceQueue<BufferedImage> {

	/**
	 * Creates a new blocking service queue with the given backing queue.
	 * @param queue The backing queue.
	 */
	public BufferedImageServiceQueue(BlockingQueue<ServicePacket<byte[]>> queue) {
		super(queue);
	}

	@Override
	protected BufferedImage transmute(byte[] data) throws TransmutationException {
		try {
			return ImageIO.read(new ByteArrayInputStream(data));
		} catch (Exception e) {
			throw new TransmutationException("Exception converting service payload into BufferedImage", e);
		}
	}
}
