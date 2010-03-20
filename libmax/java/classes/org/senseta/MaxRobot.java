package org.senseta;

import java.io.IOException;

public class MaxRobot {
	static {
		System.loadLibrary("max-java");
	}
	
	protected int handle;
	
	protected MaxRobot(int hand) {
		handle = hand;
		
		System.out.println("DEBUG: handle="+handle);
	}
	
	
	public static MaxRobot connectLocal() throws IOException {
		return new MaxRobot(connect_local());
	}
	
	
	protected static native int connect_local() throws IOException;
	protected static native Object syscall(final int handle, final String name, final String sig, Object ... args) throws IOException;
	
}
