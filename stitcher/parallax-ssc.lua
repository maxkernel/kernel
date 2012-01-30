-- Configure the Propeller SSC module. It controls the actuators on the robot
-- Created by Andrew Klofas - aklofas@gmail.com - January 2011

debug("Setting up the Propeller SSC module")

-- Load the parallax ssc module
--ssc = loadmodule("parallax-ssc")
ssc = loadmodule("ssc-32")

dm = loadmodule("drivemodel")
lm = loadmodule("lookmodel")

route(dm.front_pwm, ssc.pwm2)
route(dm.rear_pwm, ssc.pwm3)

route(lm.pan_pwm, ssc.pwm4)
route(lm.tilt_pwm, ssc.pwm5)

-- Update the motors at 10 Hz
newrategroup("Actuator update", { dm, lm, ssc }, 10)

-- Define syscalls
newsyscall("turnfront", { dm.front })
newsyscall("turnrear", { dm.rear })

newsyscall("pan", { lm.pan })
newsyscall("tilt", { lm.tilt })

--[[
newsyscall("pwm0", { ssc.pwm0 })
newsyscall("pwm1", { ssc.pwm1 })
newsyscall("pwm2", { ssc.pwm2 })
newsyscall("pwm3", { ssc.pwm3 })
newsyscall("pwm4", { ssc.pwm4 })
newsyscall("pwm5", { ssc.pwm5 })
newsyscall("pwm6", { ssc.pwm6 })
newsyscall("pwm7", { ssc.pwm7 })
newsyscall("pwm8", { ssc.pwm8 })
newsyscall("pwm9", { ssc.pwm9 })
newsyscall("pwm10", { ssc.pwm10 })
newsyscall("pwm11", { ssc.pwm11 })
newsyscall("pwm12", { ssc.pwm12 })
newsyscall("pwm13", { ssc.pwm13 })
newsyscall("pwm14", { ssc.pwm14 })
newsyscall("pwm15", { ssc.pwm15 })
--]]
--newsyscall("turnfront", { ssc.turnfront })
--newsyscall("turnrear", { ssc.turnrear })
--newsyscall("pan", { ssc.pan })
--newsyscall("tilt", { ssc.tilt })

