package org.maxkernel.service.queue;

/**
 * This exception is thrown when a requested transmutation is not supported.
 * 
 * @author Andrew Klofas
 * @version 1.0
 * 
 * @see ServiceQueue
 */
public class TransmutationNotSupportedException extends Exception {
	private static final long serialVersionUID = -4959511553909842901L;

	/**
	 * Constructs a new exception with <code>null</code> as the message.
	 */
	public TransmutationNotSupportedException() {
        super();
    }

	/**
	 * Constructs a new exception.
	 * @param message The detailed message.
	 */
    public TransmutationNotSupportedException(String message) {
        super(message);
    }

    /**
     * Constructs a new exception.
     * @param message The detailed message.
     * @param cause The cause of the exception.
     */
    public TransmutationNotSupportedException(String message, Throwable cause) {
        super(message, cause);
    }

    /**
     * Constructs a new exception with a <code>null</code> message.
     * @param cause The cause of the exception.
     */
    public TransmutationNotSupportedException(Throwable cause) {
        super(cause);
    }
}
