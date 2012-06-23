package org.maxkernel.service.priv.packet;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.ReadableByteChannel;

public class CodePacket {
	private static final int SIZE = 1;
	
	private static final int CODE_OFFSET = 0;
	
	private ByteBuffer packet;
	
	public CodePacket() {
		packet = ByteBuffer.allocate(SIZE);
		clear();
	}
	
	public boolean read(ReadableByteChannel channel) throws IOException {
		if (packet.remaining() == 0) {
			return true;
		}
		
		channel.read(packet);
		return packet.remaining() == 0;
	}
	
	public void clear() { packet.clear(); }
	public byte code() { return packet.get(CODE_OFFSET); }
}
