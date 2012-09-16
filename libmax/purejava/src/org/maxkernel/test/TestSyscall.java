package org.maxkernel.test;

import java.net.InetAddress;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;

import org.maxkernel.syscall.SyscallClient;

public class TestSyscall {

	public static void main(String[] args) throws Exception {
		SyscallClient client = new SyscallClient(InetAddress.getByName("localhost"));
		
		boolean exists = client.<Boolean>syscall("syscall_exists", "kernel_installed", "i:v");
		System.out.println(exists);
		
		long timestamp = client.<Integer>syscall("kernel_installed");
		System.out.println(timestamp);
		
		Date date = new Date(timestamp * 1000);
		System.out.println(date);
		
		client.close();
		
		/*
		// Get a syscalls iterator
		int itr = client.<Integer>syscall("syscalls_itr");
		
		// Iterate over all the available syscalls (empty string represents end)
		String name = client.<String>syscall("syscalls_next", itr);
		while (name.length() != 0) {
			
			// Get the signature of the syscall
			String signature = client.<String>syscall("syscall_signature", name);
			
			// Get the description of the syscall
			String description = client.<String>syscall("syscall_info", name);
			
			System.out.println(String.format("%s(%s) - %s", name, signature, description));
			
			// Iterate to the next syscall 
			name = client.<String>syscall("syscalls_next", itr);
		}
		
		
		// Make sure to free the iterator when done
		client.<Void>syscall("itr_free", itr);
		*/
	}

}
