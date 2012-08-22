package org.maxkernel.service.queue;

/**
 * This exception is thrown when corrupted or incorrect data is transmuted.
 * 
 * @author Andrew Klofas
 * @version 1.0
 * 
 * @see ServiceQueue
 */
public class TransmutationException extends RuntimeException {
	private static final long serialVersionUID = -4880458263423235142L;

	/**
	 * Constructs a new exception with <code>null</code> as the message.
	 */
	public TransmutationException() {
        super();
    }

	/**
	 * Constructs a new exception.
	 * @param message The detailed message.
	 */
    public TransmutationException(String message) {
        super(message);
    }

    /**
     * Constructs a new exception.
     * @param message The detailed message.
     * @param cause The cause of the exception.
     */
    public TransmutationException(String message, Throwable cause) {
        super(message, cause);
    }

    /**
     * Constructs a new exception with a <code>null</code> message.
     * @param cause The cause of the exception.
     */
    public TransmutationException(Throwable cause) {
        super(cause);
    }
}
