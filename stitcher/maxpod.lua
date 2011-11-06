-- Configure the maxpod module. Maxpod controls the actuators on the robot
-- Created by Andrew Klofas - aklofas@gmail.com - December 2010

debug("Setting up the maxpod module")

-- Load the maxpod module
maxpod = loadmodule("maxpod")
maxpod.config["serial_port"] = "/dev/ttyUSB0"

-- More will be added when maxpod is modified to utilize I/O

