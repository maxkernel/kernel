package org.maxkernel.service;

import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.util.HashMap;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

/**
 * A singleton helper class to help parse markup remotely generated by service list requests.
 * 
 * @author Andrew Klofas
 * @version 1.0
 */
public class ServiceListParser {
	private static Logger LOG = Logger.getLogger(ServiceListParser.class.getCanonicalName());
	
	private static DocumentBuilder parser = null;
	static {
		DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();

		dbf.setValidating(false);
		dbf.setIgnoringComments(true);
		dbf.setIgnoringElementContentWhitespace(true);
		//dbf.setNamespaceAware(true);
		
		try {
			parser = dbf.newDocumentBuilder();
		} catch (ParserConfigurationException e) {
			LOG.log(Level.WARNING, "Could not generate ServiceList XML Parser: " + e.getMessage());
		}
	}
	
	private ServiceListParser() {}
	
	/**
	 * Parses the given XML document into a lookup table of services.
	 * @param doc XML document
	 * @return a map of service names to service objects
	 */
	public static Map<String, Service> parseXML(Document doc) {
		Map<String, Service> services = new HashMap<String, Service>();
		
		Element root = doc.getDocumentElement();
		for (Node child = root.getFirstChild(); child != null; child = child.getNextSibling()) {
			if (child.getNodeType() == Node.ELEMENT_NODE && child.getNodeName().equals("service")) {
				try {
					NamedNodeMap attributes = child.getAttributes();
					String name = attributes.getNamedItem("name").getNodeValue();
					String format = attributes.getNamedItem("format").getNodeValue();
					String desc = attributes.getNamedItem("description").getNodeValue();
					
					services.put(name, new Service(name, format, desc));
					
				} catch (NullPointerException e) {
					LOG.log(Level.WARNING, "Malformed XML attribute. Skipping.");
					continue;
				}
			} else {
				LOG.log(Level.WARNING, "Malformed XML received. Skipping.");
				continue;
			}
		}
		
		return services;
	}
	
	/**
	 * Wrapper for {@link #parseXML(Document)} to be able to handle varying input types.
	 * @param in the inputstream to input into an XML parser
	 * @return a map of service names to service objects
	 * @throws IOException any IO errors occur
	 * @throws SAXException any parse errors occur
	 * @see #parseXML(Document)
	 */
	public static Map<String, Service> parseXML(InputStream in) throws IOException, SAXException {
		if (parser == null) {
			throw new SAXException("SAX Parser not defined!");
		}
		
		return parseXML(parser.parse(in));
	}
	
	/**
	 * Wrapper for {@link #parseXML(Document)} to be able to handle varying input types.
	 * @param reader the XML document reader
	 * @return a map of service names to service objects
	 * @throws IOException any IO errors occur
	 * @throws SAXException any parse errors occur
	 * @see #parseXML(Document)
	 */
	public static Map<String, Service> parseXML(Reader reader) throws IOException, SAXException{
		if (parser == null) {
			throw new SAXException("SAX Parser not defined!");
		}
		
		return parseXML(parser.parse(new InputSource(reader)));
	}
}