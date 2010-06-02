'''
Created on Jun 2, 2010

@author: dklofas
'''

import threading
import socket
import time
import re
import logging
import uuid

# Global variables
HOST = ''
PORT = 8080
WWW = "www"
EXPIRE = 24 * 60 * 60
LOG = logging.getLogger("scienceserver")
PASSFILE = "users.txt"
SESSIONS = {}

# Global configuration
logging.basicConfig(level=logging.DEBUG)

def headers(conn, type):
    conn.send("HTTP/1.0 200 OK\r\nContent-Type: "+type+"\r\n\r\n")

def login(user, passwd):
    try:
        with open(PASSFILE, 'r') as f:
            while True:
                line = f.readline()
                if not line:
                    break
                
                c = re.match("^([a-zA-Z0-9_\\-]*):(.*)$", line)
                if not c:
                    continue
                
                if user == c.group(1) and passwd == c.group(2):
                    return True
            
            return False
    
    except IOError:
        LOG.error("User credentials file '"+PASSFILE+"' doesn't exist!")
        return True

def session(user):
    if user == '':
        user = "Guest"
    
    class sessionc:
        pass
    
    s = sessionc()
    s.user = user
    s.uid = uuid.uuid4()
    s.expires = time.time() + EXPIRE
    SESSIONS[str(s.uid)] = s
    
    LOG.info("Created session for user '"+user+"' with uid "+str(s.uid))
    return str(s.uid)

def bgtasks():
    while True:
        
        # Delete stale features
        for v in SESSIONS.keys():
            if SESSIONS[v].expires < time.time():
                LOG.info("Deleting stale session: '"+SESSIONS[v].user+"', "+str(SESSIONS[v].uid))
                del SESSIONS[v]
        
        # Sleep for a minute
        time.sleep(60)

def handlewww(conn, addr, url, proto, header):
    if proto != "HTTP":
        return
    
    if url == "/":
        url = "/index.html"
    
    try:
        with open(WWW+url, 'r') as fp:
            # Send the http headers
            s = re.match("^.*\\.([a-z]+)$", url)
            if s:
                headers(conn, {
                    'png': "image/png",
                    'gif': "image/gif",
                    'jpg': "image/jpeg"
                }.get(s.group(1), "text/html"))
            
            # Now send the data
            conn.send(fp.read())
    
    except IOError:
        LOG.warn("Could not service www request for '"+url+"'")

def handleactivation(conn, addr, id, proto, header, target):
    if proto != "TEXT":
        return
    
    conn.send("challenge\r\n")
    while True:
        data = conn.recv(512)
        
        if not data:
            return
        
        c = re.match("^user=(.*)&pass=(.*)\r\n$", data)
        if not c or not login(c.group(1), c.group(2)):
            conn.send("invalid\r\n")
            continue
        
        # Valid password
        user = c.group(1)
        break
    
    id = session(user)
    conn.send("id="+id+"\r\n")

def handleerror(conn, addr, id, proto, header, target):
    LOG.warn("Invalid request for /"+target+"/"+str(id))
    if proto == "HTTP":
        headers(conn, "text/html")
        conn.send("<html><head><title>Bad request</title></head><body><h1>Error:</h1>Bad request</body></html>")

def handlerequest(conn, addr):
    d = conn.recv(2048).splitlines()
    header = {}
    get = ""
    proto = ""
    for v in d:
        g = re.match("^GET (.*) ((?:HTTP)|(?:TEXT)|(?:ATP)).*$", v)
        h = re.match("^([a-zA-Z0-9\\-]+): (.*)$", v)
        if g:
            get = g.group(1)
            proto = g.group(2)
        elif h:
            header[h.group(1)] = h.group(2)
    
    t = re.match("^/([a-z_]+)(?:/([a-zA-Z0-9_\\-\\./]+))?$", get)
    if not t:
        handlewww(conn, addr, get, proto, header)
        
    else:
        target = t.group(1)
        id = t.group(2)
        
        {
            'activation': handleactivation 
        }.get(target, handleerror)(conn, addr, id, proto, header, target)
    
    conn.close()


if __name__ == '__main__':
    
    # Start bg tasks
    bg = threading.Thread(target=bgtasks, name="Background Tasks")
    bg.daemon = True
    bg.start()
    
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((HOST, PORT))
    server.listen(1)
    
    try:
        
        while True:
            conn, addr = server.accept()
            LOG.info("Client connected "+str(addr))
            t = threading.Thread(target=handlerequest, args=[conn, addr])
            t.daemon = True
            t.start()
            
    except KeyboardInterrupt:
        LOG.info("Quitting due to keyboard interrupt")
    except:
        LOG.warn("Quitting due to unknown exception")
    
    server.close()