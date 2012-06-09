-- Configure the webcam modules. Create the video pipeline
-- Created by Andrew Klofas - aklofas@gmail.com - December 2010

debug("Setting up the webcam modules")

-- Set up the webcam module
webcam_module = loadmodule("webcam")
camera = webcam_module.webcam("/dev/video0", "YUV422", 320, 240)


-- Set up the JPEG compressor module
jpeg_module = loadmodule("jpegcompress")
jpegcompress = jpeg_module.compressor("YUV422", 80)


-- Set up the service module
service_module = loadmodule("service")
camerasink = service_module.bufferstream("video", "Camera video stream", "JPEG", "Jpeg frame from the onboard webcam")


-- Route the data
route(camera.width, jpegcompress.width)
route(camera.height, jpegcompress.height)
route(camera.frame, jpegcompress.frame)
route(jpegcompress.frame, camerasink.buffer)


-- Create the execution thread
pipeline = newrategroup("Camera pipeline", { camera, jpegcompress, camerasink }, 30)


-- Allow us to muck with the width/height and framerate through a syscall
newsyscall("videoparams", "v:iii", {camera.width, camera.height, pipeline.rate}, nil, "Configures the video pipeline. (1) Frame resolution width. (2) Frame resolution height. (3) Frames per second.")


