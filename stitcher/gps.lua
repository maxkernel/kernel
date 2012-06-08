debug("Setting up the GPS script")

gps_module = loadmodule("gps")

gps = gps_module.device("/dev/ttyUSB0", 4800)

-- Define syscalls
newsyscall("gps_latitude", "d:v", { }, gps.latitude, "Returns the GPS latitude")
newsyscall("gps_longitude", "d:v", { }, gps.longitude, "Returns the GPS longitude")
newsyscall("gps_haslock", "b:v", { }, gps.lock, "Returns true if there is a GPS lock")

--newrategroup("GPS Update", { gps }, 2)


