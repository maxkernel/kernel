package org.maxkernel.service;

public class Service {
	private String name, format, desc;
	
	public Service(String name, String format, String desc) {
		this.name = name;
		this.format = format;
		this.desc = desc;
	}
	
	public Service(String name, String format) {
		this(name, format, null);
	}
	
	public Service(String name) {
		this(name, "Unknown");
	}
	
	public String name() { return name; }
	public String format() { return format; }
	public String description() { return desc; }
	
	public boolean equals(Object o) {
		return o instanceof Service && ((Service)o).name() == name() && ((Service)o).format() == format();
	}
	
	public String toString() {
		return "Service name='"+name()+"' format='"+format()+"' description='"+description()+"'";
	}
}
