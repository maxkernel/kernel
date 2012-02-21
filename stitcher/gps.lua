debug("Setting up the GPS module")

gps_module = loadmodule("gps")

gps = gps_module.device.new("/dev/ttyUSB0", 4800)

-- Define syscalls
newsyscall("gps_latitude", { }, gps.latitude)
newsyscall("gps_longitude", { }, gps.longitude)
newsyscall("gps_haslock", { }, gps.lock)

newrategroup("GPS Update", { gps }, 2)


