package org.senseta.event;

import org.senseta.client.ServiceClient.Protocol;

public interface ServiceListener {
	public void newFrame(String service, Protocol p, long timestamp_us, byte[] data);
}
