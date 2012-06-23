package org.maxkernel.service.priv.packet;

import java.io.EOFException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ReadableByteChannel;

public class DataPacket {
	private static final int HEADER_SIZE = 12;
	private static final int BODY_SIZE = 256;
	
	private static final int TIMESTAMP_OFFSET = 0;
	private static final int SIZE_OFFSET = 8;
	
	private ByteBuffer header, body;
	
	private byte[] payload;
	private int index;
	
	public DataPacket() {
		header = ByteBuffer.allocate(HEADER_SIZE).order(ByteOrder.LITTLE_ENDIAN);
		body = ByteBuffer.allocateDirect(BODY_SIZE);
		clear();
	}
	
	public boolean read(ReadableByteChannel channel) throws IOException {
		if (header.remaining() > 0) {
			// Read in the header
			int read = channel.read(header);
			if (read < 0) {
				throw new EOFException();
			}
		}
		
		if (header.remaining() == 0) {
			if (payload == null) {
				payload = new byte[header.getInt(SIZE_OFFSET)];
				index = 0;
			}
			
			if (index == payload.length) {
				return true;
			}
			
			body.clear();
			body.limit(Math.min(body.capacity(), payload.length - index));
			int read = channel.read(body);
			body.flip();
			
			if (read < 0) {
				throw new EOFException();
			}
			
			body.get(payload, index, read);
			index += read;
			
			return index == payload.length;
		}
		
		return false;
	}
	
	public void clear() { header.clear(); payload = null; }
	public long timestamp() { return header.getLong(TIMESTAMP_OFFSET); }
	public int size() { return header.getInt(SIZE_OFFSET); }
	public byte[] payload() { return payload; }
}
