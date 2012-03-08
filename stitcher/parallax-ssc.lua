-- Configure the Propeller SSC module. It controls the actuators on the robot
-- Created by Andrew Klofas - aklofas@gmail.com - January 2011

debug("Setting up the Propeller SSC module")

-- Load the parallax ssc module
--ssc = loadmodule("parallax-ssc")
ssc = loadmodule("ssc-32")

dm = loadmodule("drivemodel")
lm = loadmodule("lookmodel")

route(dm.motor_front_pwm, ssc.pwm0)
route(dm.front_pwm, ssc.pwm2)
route(dm.rear_pwm, ssc.pwm3)

route(lm.pan_pwm, ssc.pwm4)
route(lm.tilt_pwm, ssc.pwm5)

-- Update the motors at 10 Hz
newrategroup("Actuator update", { dm, lm, ssc }, 10)

-- Define syscalls
newsyscall("motor", { dm.throttle }, nil)
newsyscall("turnfront", { dm.front }, nil)
newsyscall("turnrear", { dm.rear }, nil)

newsyscall("pan", { lm.pan }, nil)
newsyscall("tilt", { lm.tilt }, nil)


