package org.maxkernel.test;

import java.net.InetAddress;

import org.maxkernel.syscall.SyscallClient;

public class TestSyscall {

	public static void main(String[] args) throws Exception {
		SyscallClient client = new SyscallClient(InetAddress.getByName("localhost"));
		
		System.out.println(client.syscall("kernel_installed"));
	}

}
