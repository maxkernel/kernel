-- A sample lua script that is meant to configure a Max5(R/J)
-- Created by Andrew Klofas - aklofas@gmail.com - August 2009

debug("Control has been passed to Lua")

path=path .. ":/home/dklofas/projects/maxkernel/src/videoserver:/home/dklofas/projects/maxkernel/src/videotools:/home/dklofas/projects/maxkernel/src/jpegcompress:/home/dklofas/projects/maxkernel/src/x264compress:/home/dklofas/projects/maxkernel/src/nimu"

-- Load driver for the IO Board
maxpod = loadmodule("maxpod")
--mssc = loadmodule("pololu-mssc")
--mssc.config["serial_port"] = "/dev/ttyUSB0"

webcam = loadmodule("webcam")
----left = webcam.device.new("/dev/video0", "YUV420", 640, 480)
left = webcam.device.new("/dev/video0", "YUV422", 320, 240)

jpeg = loadmodule("jpegcompress")
left_jpeg = jpeg.compressor.new("YUV422")

--videoserver = loadmodule("videoserver")
--left_srv = videoserver.tcp.new(20302)

service = loadmodule("service")

left_srv = service.bufferstream.new("video", "Left camera video stream", "JPEG", "description");


route(left.width, left_jpeg.width)
route(left.height, left_jpeg.height)
route(left.frame, left_jpeg.frame)

route(left_jpeg.frame, left_srv.buffer)

rg = newvrategroup("Camera pipeline", { left, left_conv, left_jpeg, left_srv }, nil, 10)

newsyscall("videoparams", {left.width, left.height, rg.rate})

--loadmodule("nimu")

--test = loadmodule("test")
--blk = test.myblock.new()
--rg = newvrategroup("Test pipeline", {blk}, nil, 1)
--newsyscall("setrate", {rg.rate})

--newsyscall("setvar", {blk.myblock_in})

