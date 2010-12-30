-- A sample lua script that is meant to configure a Max5(R/J)
-- Created by Andrew Klofas - aklofas@gmail.com - August 2009

debug("Control has been passed to Lua")

path=path .. ":/home/dklofas/Projects/maxkernel/src/miscserver:/home/dklofas/Projects/maxkernel/src/videotools:/home/dklofas/Projects/maxkernel/src/jpegcompress:/home/dklofas/Projects/maxkernel/src/x264compress:/home/dklofas/Projects/maxkernel/src/nimu:/home/dklofas/Projects/maxkernel/src/network:/home/dklofas/Projects/maxkernel/src/gps"

-- Load Maxpod modules
maxpod = loadmodule("maxpod")


-- Load GPS modules
gps = loadmodule("gps")
service = loadmodule("service")
gps_service = service.bufferstream.new("gps", "GPS data stream", "RAW", "description")

route(gps.stream, gps_service.buffer)

newrategroup("GPS Pipeline", { gps, gps_service }, 1)

-- Load video modules
webcam = loadmodule("webcam")
jpeg = loadmodule("jpegcompress")
service = loadmodule("service")
left = webcam.device.new("/dev/video0", "YUV422", 640, 480)
left_jpeg = jpeg.compressor.new("YUV422", 80)
left_srv = service.bufferstream.new("video", "Left camera video stream", "JPEG", "description")

route(left.width, left_jpeg.width)
route(left.height, left_jpeg.height)
route(left.frame, left_jpeg.frame)
route(left_jpeg.frame, left_srv.buffer)

rg = newvrategroup("Camera pipeline", { left, left_jpeg, left_srv }, nil, 7)
newsyscall("videoparams", {left.width, left.height, rg.rate})


-- Load wireless modules
--wifi = loadmodule("network")
--service = loadmodule("service")
--wifi_srv = service.bufferstream.new("wifi_ss", "Wifi signal strength", "RAW", "wifi service")

--route(wifi.strength, wifi_srv.buffer)

--newrategroup("Wifi pipeline", {wifi, wifi_srv}, 2)




--test = loadmodule("test")
--blk = test.myblock.new()
--rg = newvrategroup("Test pipeline", {blk}, nil, 1)
--newsyscall("setrate", {rg.rate})

