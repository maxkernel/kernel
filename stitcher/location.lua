-- Configure the locationing modules on the robot
-- Created by Andrew Klofas - andrew@maxkernel.com - June 2012

debug("Setting up the locationing pipeline")

gps_module = loadmodule("gps")

gps = gps_module.nmea("/dev/ttyUSB0", 4800)

-- Define syscalls
newsyscall("gps_latitude", "d:v", gps.latitude, nil, "Returns the GPS latitude")
newsyscall("gps_longitude", "d:v", gps.longitude, nil, "Returns the GPS longitude")
newsyscall("gps_haslock", "b:v", gps.lock, nil, "Returns true if there is a GPS lock")

newrategroup("GPS Update", 0, { gps }, 2)

