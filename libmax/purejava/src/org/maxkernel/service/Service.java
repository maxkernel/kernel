package org.maxkernel.service;

import org.maxkernel.service.streams.Stream;

/**
 * Encapsulates a service type. Used to in streams subscribe to specific services.
 * @see Stream#listServices()
 * @see Stream#subscribe(Service)
 * 
 * @version 1.0
 * @author Andrew Klofas
 */
public class Service {
	private String name, format, desc;
	
	/**
	 * Creates a new Service object descriptor.
	 * @param name Name of the service. Can include spaces and special characters 
	 * @param format Format of the service. One word or acronym, case-insensitive
	 * @param desc Optional human-readable description of the service. Can be <code>null</code> 
	 */
	public Service(String name, String format, String desc) {
		this.name = name;
		this.format = format.toLowerCase();
		this.desc = desc;
	}
	
	/**
	 * Creates a new Service object descriptor with a <code>null</code> description.
	 * @param name Name of the service. Can include spaces and special characters
	 * @param format Format of the service. One word or acronym, case-insensitive
	 */
	public Service(String name, String format) {
		this(name, format, null);
	}
	
	/**
	 * Creates a new Service object descriptor with an "unknown" format and <code>null</code> description.
	 * @param name Name of the service. Can include spaces and special characters
	 */
	public Service(String name) {
		this(name, "unknown");
	}
	
	/**
	 * @return The name of the service
	 */
	public String getName() {
		return name;
	}
	
	/**
	 * @return The format of the service, or "unknown" if not specified
	 */
	public String getFormat() {
		return format;
	}
	
	/**
	 * @return The description of the service, or <code>null</code> if not specified
	 */
	public String getDescription() {
		return desc;
	}
	
	@Override
	public boolean equals(Object o) {
		return o instanceof Service && ((Service)o).name.equals(name) && ((Service)o).format.equals(format);
	}
	
	@Override
	public int hashCode() {
		return name.hashCode();
	}
	
	@Override
	public String toString() {
		return String.format("Service name='%s' format='%s' description='%s'", name, format, desc);
	}
}
