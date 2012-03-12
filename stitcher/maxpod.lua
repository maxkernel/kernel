-- Configure the maxpod module. Maxpod controls the actuators on the robot
-- Created by Andrew Klofas - aklofas@gmail.com - December 2010

debug("Setting up the maxpod module")

-- Load the maxpod module
--[[
maxpod = loadmodule("maxpod")
maxpod.config["serial_port"] = "/dev/ttyUSB0" --"/dev/ttyS0"


dm = loadmodule("drivemodel")
lm = loadmodule("lookmodel")

route(dm.motor_front_pwm, maxpod.pwm0)
route(dm.front_pwm, maxpod.pwm2)
route(dm.rear_pwm, maxpod.pwm3)

route(lm.pan_pwm, maxpod.pwm4)
route(lm.tilt_pwm, maxpod.pwm5)

-- Update the motors at 10 Hz
newrategroup("Actuator update", { dm, lm, maxpod }, 10)

-- Define syscalls
newsyscall("motor", { dm.throttle }, nil)
newsyscall("turnfront", { dm.front }, nil)
newsyscall("turnrear", { dm.rear }, nil)

newsyscall("pan", { lm.pan }, nil)
newsyscall("tilt", { lm.tilt }, nil)
--]]

