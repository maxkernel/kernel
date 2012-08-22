package org.maxkernel.syscall;

/**
 * An exception occured while executing syscall on the remote side.
 * More information will be in the message and possible cause.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class SyscallRemoteException extends SyscallException {
	private static final long serialVersionUID = -4558532187624995508L;
	public SyscallRemoteException() { super(); }
	public SyscallRemoteException(String message) { super(message); }
	public SyscallRemoteException(Throwable cause) { super(cause); }
	public SyscallRemoteException(String message, Throwable cause) { super(message, cause); }
}
