package org.maxkernel.syscall;

public class SyscallNotFoundException extends SyscallException {
	private static final long serialVersionUID = -4899461583382330844L;
	public SyscallNotFoundException(String message) { super(message); }
	public SyscallNotFoundException(Throwable cause) { super(cause); }
	public SyscallNotFoundException(String message, Throwable cause) { super(message, cause); }
}
