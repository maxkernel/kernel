package org.maxkernel.syscall;

public class SyscallException extends Exception {
	private static final long serialVersionUID = 1288689787905672654L;
	public SyscallException(String message) { super(message); }
	public SyscallException(Throwable cause) { super(cause); }
	public SyscallException(String message, Throwable cause) { super(message, cause); }
}
