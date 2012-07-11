package org.maxkernel.service.format;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.concurrent.BlockingQueue;

import javax.imageio.ImageIO;

import org.maxkernel.service.ServicePacket;

public class BufferedImageFormat implements ServiceFormat<BufferedImage> {
	
	private BlockingQueue<ServicePacket> queue;
	
	public BufferedImageFormat(BlockingQueue<ServicePacket> queue) {
		this.queue = queue;
	}
	
	public Payload<BufferedImage> dequeue() throws IOException, InterruptedException {
		ServicePacket p = queue.take();
		return new Payload<BufferedImage>(p.timestamp(), ImageIO.read(new ByteArrayInputStream(p.data())));
	}
}
