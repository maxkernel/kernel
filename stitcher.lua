-- A sample lua script that is meant to configure a Max5(R/J)
-- Created by Andrew Klofas - aklofas@gmail.com - August 2009

debug("Control has been passed to Lua")

path=path .. ":/home/dklofas/projects/maxkernel/src/miscserver:/home/dklofas/projects/maxkernel/src/videotools:/home/dklofas/projects/maxkernel/src/jpegcompress:/home/dklofas/projects/maxkernel/src/x264compress:/home/dklofas/projects/maxkernel/src/nimu:/home/dklofas/projects/maxkernel/src/network"



-- Load driver for the IO Board
maxpod = loadmodule("maxpod")
--mssc = loadmodule("pololu-mssc")
--mssc.config["serial_port"] = "/dev/ttyUSB0"

webcam = loadmodule("webcam")
----left = webcam.device.new("/dev/video0", "YUV420", 640, 480)
left = webcam.device.new("/dev/video0", "YUV422", 640, 480)

jpeg = loadmodule("jpegcompress")
left_jpeg = jpeg.compressor.new("YUV422", 80)

miscserver = loadmodule("miscserver")
left_srv = miscserver.jpegproxy.new("www.maxkernel.com", 8089)

service = loadmodule("service")

--left_srv = service.bufferstream.new("video", "Left camera video stream", "JPEG", "description");


route(left.width, left_jpeg.width)
route(left.height, left_jpeg.height)
--route(left.frame, left_jpeg.frame)

route(left.frame, left_jpeg.frame)
route(left_jpeg.frame, left_srv.frame)

rg = newvrategroup("Camera pipeline", { left, left_jpeg, left_srv }, nil, 7)

--newsyscall("videoparams", {left.width, left.height, rg.rate})

--loadmodule("nimu")

--test = loadmodule("test")
--blk = test.myblock.new()
--rg = newvrategroup("Test pipeline", {blk}, nil, 1)
--newsyscall("setrate", {rg.rate})

--newsyscall("setvar", {blk.myblock_in})


--net = loadmodule("network")
--newrategroup("Network", { net }, 2)

