package org.maxkernel.syscall;

/**
 * Named syscall does not exist exception.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class SyscallNotFoundException extends SyscallException {
	private static final long serialVersionUID = -4899461583382330844L;
	public SyscallNotFoundException() { super(); }
	public SyscallNotFoundException(String message) { super(message); }
	public SyscallNotFoundException(Throwable cause) { super(cause); }
	public SyscallNotFoundException(String message, Throwable cause) { super(message, cause); }
}
