package org.maxkernel.service;

public class Service {
	private String name, format, desc;
	
	public Service(String name, String format, String desc) {
		this.name = name;
		this.format = format;
		this.desc = desc;
	}
	
	public String getName() { return name; }
	public String getFormat() { return format; }
	public String getDescription() { return desc; }
	
	public String toString() {
		return "Service name='"+getName()+"' format='"+getFormat()+"' description='"+getDescription()+"'";
	}
}
