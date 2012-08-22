package org.maxkernel.service.queue;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;

import org.maxkernel.service.ServiceClient;
import org.maxkernel.service.ServicePacket;

/**
 * This is a superclass designed to be inherited by classes that transform raw service data
 * into more usable forms. Initially, service payloads are represented by a <code>byte[]</code>.
 * This is very unwieldy if the data is actually, say, a <code>double[]</code> or a BufferedImage.
 * To transmute the data, this class wraps a blocking queue (<code>BlockingQueue<ServicePacket<byte[]>></code>)
 * normally registered with the ServiceClient
 * ({@link ServiceClient#begin(org.maxkernel.service.streams.Stream, BlockingQueue)})
 * with a friendlier BlockingQueue that returns the requested transmuted service packets.
 * 
 * @author Andrew Klofas
 * @version 1.0
 *
 * @param <E> The class of the payload data in the {@link ServicePacket}
 * @see BooleanArrayServiceQueue
 * @see IntegerArrayServiceQueue
 * @see DoubleArrayServiceQueue
 * @see BufferedImageServiceQueue
 */
public abstract class ServiceQueue<E> implements BlockingQueue<ServicePacket<E>> {
	
	/**
	 * The blocking queue that holds the raw binary un-transmuted {@link ServicePacket}.
	 */
	protected BlockingQueue<ServicePacket<byte[]>> backingQueue;
	
	/**
	 * Registers the given blocking queue as the backing queue.
	 * @param queue
	 */
	protected ServiceQueue(BlockingQueue<ServicePacket<byte[]>> queue) {
		this.backingQueue = queue;
	}
	
	/**
	 * @return the un-tansmuted backing blocking queue.
	 */
	public BlockingQueue<ServicePacket<byte[]>> getBackingQueue() {
		return backingQueue;
	}
	
	/**
	 * Abstract method overridden in inherited classes to convert raw binary blob to
	 * usable class.
	 * @param data The raw un-tansmuted data.
	 * @return The transmuted data.
	 * @throws TransmutationException if the data is corrupted or cannot be transmuted.
	 */
	protected abstract E transmute(byte[] data) throws TransmutationException;


	@Override
	public ServicePacket<E> take() throws InterruptedException, TransmutationException {
		ServicePacket<byte[]> p = backingQueue.take();
		return new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
	}
	
	@Override
	public ServicePacket<E> remove() throws TransmutationException {
		ServicePacket<byte[]> p = backingQueue.remove();
		return new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
	}

	@Override
	public ServicePacket<E> poll() throws TransmutationException {
		ServicePacket<byte[]> p = backingQueue.poll();
		return new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
	}
	
	@Override
	public ServicePacket<E> poll(long timeout, TimeUnit unit) throws InterruptedException, TransmutationException {
		ServicePacket<byte[]> p = backingQueue.poll(timeout, unit);
		return new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
	}

	@Override
	public ServicePacket<E> element() throws TransmutationException {
		ServicePacket<byte[]> p = backingQueue.element();
		return new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
	}

	@Override
	public ServicePacket<E> peek() throws TransmutationException {
		ServicePacket<byte[]> p = backingQueue.peek();
		return new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
	}
	
	

	@Override
	public int size() {
		return backingQueue.size();
	}
	
	@Override
	public int remainingCapacity() {
		return 0;
	}

	@Override
	public boolean isEmpty() {
		return backingQueue.isEmpty();
	}
	
	@Override
	public void clear() {
		backingQueue.clear();
	}

	
	
	@Override
	public int drainTo(Collection<? super ServicePacket<E>> c) {
		return drainTo(c, Integer.MAX_VALUE);
	}

	@Override
	public int drainTo(Collection<? super ServicePacket<E>> c, int maxElements) {
		if (c == null)
            throw new NullPointerException();
        if (c == this)
            throw new IllegalArgumentException();
        
        
        ArrayList<ServicePacket<byte[]>> un = new ArrayList<>();
        backingQueue.drainTo(un, maxElements);
        
        for (ServicePacket<byte[]> p : un) {
        	c.add(new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData())));
        }
        
        return un.size();
	}

	@Override
	@SuppressWarnings("unchecked")
	public Object[] toArray() throws TransmutationException {
		Object[] un = backingQueue.toArray();
		Object[] c = new Object[un.length];
		
		for (int i=0; i<un.length; i++)
		{
			ServicePacket<byte[]> p = (ServicePacket<byte[]>)un[i];
			c[i] = new ServicePacket<E>(p.getService(), p.getStream(), p.getTimestamp(), transmute(p.getData()));
		}
		
		return c;
	}

	@Override
	@SuppressWarnings("unchecked")
	public <T> T[] toArray(T[] a) throws TransmutationException {
		ServicePacket<byte[]>[] un = backingQueue.toArray(new ServicePacket[0]);
		int count = un.length;
		
		if (a.length < count) {
            a = (T[])Array.newInstance(a.getClass().getComponentType(), count);
		}
		
		for (int i=0; i<count; i++) {
			a[i] = (T)new ServicePacket<E>(un[i].getService(), un[i].getStream(), un[i].getTimestamp(), transmute(un[i].getData()));
		}
		
		if (count < a.length) {
			a[count] = null;
		}
		
		return a;
	}

	
	
	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public Iterator<ServicePacket<E>> iterator() {
		throw new UnsupportedOperationException("Cannot iterate over elements in this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean containsAll(Collection<?> c) {
		throw new UnsupportedOperationException("Cannot query elements out of order in this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean addAll(Collection<? extends ServicePacket<E>> c) {
		throw new UnsupportedOperationException("Cannot add elements to this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean removeAll(Collection<?> c) {
		throw new UnsupportedOperationException("Cannot remove elements out of order in this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean retainAll(Collection<?> c) {
		throw new UnsupportedOperationException("Cannot query elements out of order in this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean add(ServicePacket<E> e) {
		throw new UnsupportedOperationException("Cannot add elements to this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean offer(ServicePacket<E> e) {
		throw new UnsupportedOperationException("Cannot offer elements to this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean offer(ServicePacket<E> e, long timeout, TimeUnit unit) throws InterruptedException {
		throw new UnsupportedOperationException("Cannot offer elements to this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public void put(ServicePacket<E> e) throws InterruptedException {
		throw new UnsupportedOperationException("Cannot put elements in this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean remove(Object o) {
		throw new UnsupportedOperationException("Cannot remove elements out of order in this queue");
	}

	/**
	 * Method operation not supported. Will throw an UnsupportedOperationException if invoked.
	 */
	@Override
	public boolean contains(Object o) {
		throw new UnsupportedOperationException("Cannot query elements out of order in this queue");
	}
}
