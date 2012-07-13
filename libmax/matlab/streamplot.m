

%% Initialize our environment
clear;

%% Add the JAVA library to MATLAB path
javaclasspath('/home/dklofas/Projects/maxkernel/kernel/libmax/purejava/maxkernel.jar')
addpath('/home/dklofas/Projects/maxkernel/kernel/libmax/matlab/max')

%% Initialize a TCP stream

% Create the service client. Right now, there are streams attached, so the
% client is just idly waiting doing nothing
client = serviceclient();

% Here, we create a tcp stream and connect to localhost using the default port
tcpstream = servicestream('tcp', 'localhost');

% Now list the available services
services = tcpstream.services();
display('Available services:');
display(services);

%% Connect to a service

NAME = 'Camera video stream';

% To subscribe to a service, use the service object from the map returned
% from the services() call
tcpstream.subscribe(NAME);

%% Start streaming

% This will create a blocking queue and begin streaming the data from the
% robot. Be sure to quickly start consuming from the queue, especially for
% fast frequency services
queue = client.begin(tcpstream);

%% Display the data real-time

figure(1);

% x = [];
% y = [];
% h = line(nan, nan);
% 
% for i = 1:500
%     packet = queue.dequeue();
%     
%     timestamp = packet.timestamp();
%     data = packet.data();
%     
%     x = [x timestamp];
%     y = [y data(1)];
%     set(h, 'XData', x, 'YData', y);
%     drawnow;
% end

for n = 1:500
    packet = queue.dequeue();
    img = packet.data();
    
    % Convert from java BufferedImage to native MATLAB image
    w=img.getWidth();
    h=img.getHeight();
    b = uint8(zeros([h,w,3]));
    pixels = uint8(img.getData().getPixels(0,0,w,h,[]));
    for i = 1 : h
        base = (i-1)*w*3+1;
        b(i,1:w,:) = deal(reshape(pixels(base:(base+3*w-1)),3,w)');
    end
    
    image(b);
    drawnow;
end

%% Clean up

% Close the tcp stream
tcpstream.close();

% Empty the queue
queue.clear();

% Close the service client
client.close();
