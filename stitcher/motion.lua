-- Configure the robotic motion that controls the actuators on the robot
-- Created by Andrew Klofas - andrew@maxkernel.com - June 2012

debug("Setting up the motion pipeline")

ssc_module = loadmodule("ssc")
mm_module = loadmodule("motionmodel")

ssc32 = ssc_module.ssc32("/dev/ttyUSB0")
dm = mm_module.quadrasteer()
lm = mm_module.pantilt()

route(dm.motor_front_pwm, ssc32.pwm0)
route(dm.front_pwm, ssc32.pwm2)
route(dm.rear_pwm, ssc32.pwm3)

route(lm.pan_pwm, ssc32.pwm4)
route(lm.tilt_pwm, ssc32.pwm5)

-- Update the motors at 10 Hz
newrategroup("Actuator update", 1, { dm, lm, ssc32 }, 10)

-- Define syscalls
newsyscall("motor", "v:d", nil, { dm.throttle }, "Motor desc")
newsyscall("turnfront", "v:d", nil, { dm.front }, "Turnfront desc")
newsyscall("turnrear", "v:d", nil, { dm.rear }, "Turnrear desc")

newsyscall("pan", "v:d", nil, { lm.pan }, "Pan desc")
newsyscall("tilt", "v:d", nil, { lm.tilt }, "Tilt desc")


