-- Configure the webcam modules. Create the video pipeline
-- Created by Andrew Klofas - aklofas@gmail.com - December 2010

debug("Setting up the webcam modules")

-- Set up the webcam module
webcam = loadmodule("webcam")
camera = webcam.device.new("/dev/video0", "YUV422", 320, 240)


-- Set up the JPEG compressor module
jpeg = loadmodule("jpegcompress")
jpegcompress = jpeg.compressor.new("YUV422", 80)


-- Set up the service module
service = loadmodule("service")
camerasink = service.bufferstream.new("video", "Camera video stream", "JPEG", "Jpeg frame from the onboard webcam")


-- Route the data
route(camera.width, jpegcompress.width)
route(camera.height, jpegcompress.height)
route(camera.frame, jpegcompress.frame)
route(jpegcompress.frame, camerasink.buffer)


-- Create the execution thread
pipeline = newrategroup("Camera pipeline", { camera, jpegcompress, camerasink }, 30)


-- Allow us to muck with the width/height and framerate through a syscall
newsyscall("videoparams", {camera.width, camera.height, pipeline.rate}, nil)


