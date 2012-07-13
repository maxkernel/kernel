package org.maxkernel.syscall;

public class SyscallRemoteException extends SyscallException {
	private static final long serialVersionUID = -4558532187624995508L;
	public SyscallRemoteException(String message) { super(message); }
	public SyscallRemoteException(Throwable cause) { super(cause); }
	public SyscallRemoteException(String message, Throwable cause) { super(message, cause); }
}
