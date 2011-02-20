-- Configure the Propeller SSC module. It controls the actuators on the robot
-- Created by Andrew Klofas - aklofas@gmail.com - January 2011

debug("Setting up the Propeller SSC module")

-- Load the propeller ssc module
ssc = loadmodule("propeller-ssc")

-- Update the motors at 10 Hz
newrategroup("Actuator update", { ssc }, 10)

-- Define syscalls
newsyscall("motor", { ssc.motor })
newsyscall("turnfront", { ssc.turnfront })
newsyscall("turnrear", { ssc.turnrear })
newsyscall("pan", { ssc.pan })
newsyscall("tilt", { ssc.tilt })

