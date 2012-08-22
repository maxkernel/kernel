package org.maxkernel.syscall;

/**
 * Base class for exceptions while executing remote syscalls. 
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class SyscallException extends Exception {
	private static final long serialVersionUID = 1288689787905672654L;
	
	/**
	 * Constructs a new exception with <code>null</code> as the message.
	 */
	public SyscallException() {
		super();
	}
	
	/**
	 * Constructs a new exception.
	 * @param message The detailed message.
	 */
	public SyscallException(String message) {
		super(message);
	}
	
	/**
     * Constructs a new exception with a <code>null</code> message.
     * @param cause The cause of the exception.
     */
	public SyscallException(Throwable cause) {
		super(cause);
	}
	
	/**
     * Constructs a new exception.
     * @param message The detailed message.
     * @param cause The cause of the exception.
     */
	public SyscallException(String message, Throwable cause) {
		super(message, cause);
	}
}
