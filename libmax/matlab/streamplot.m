

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
tcpstream = servicestream('tcp', '192.168.2.4');

% Now list the available services
services = tcpstream.services();
display('Available services:');
display(services);

%% Connect to a service

NAME = 'lidar';

% To subscribe to a service, use the service object from the map returned
% from the services() call
tcpstream.subscribe(NAME);

%% Start streaming

% This will create a blocking queue and begin streaming the data from the
% robot. Be sure to quickly start consuming from the queue, especially for
% fast frequency services
queue = client.begin(tcpstream);

%% Display the data real-time

x = [];
y = [];

figure(1);
h1 = line(nan, nan);

figure(2);
h2 = line(nan, nan);

%
s = syscallclient('192.168.2.3');
sp = [];
pheight = 1000;
%

for i = 1:100000
    packet = queue.dequeue();
    
    timestamp = packet.timestamp();
    data = packet.data();
    
    if data(2) < -5000
        data(2) = 0;
    end
    
    x = [x timestamp];
    y = [y data(1)];
    sp = [sp data(2)-1500];
    yfilt = filter(Hd, y);
    
    %speed = sin(pi/10 * i) * 500;
    
    height = (yfilt(length(yfilt)) - 1000) / 500;
    %height = (y(length(y)) - 1000) / 500;
    speed = (height * 250) + (height - pheight) * 200;
    
    pheight = height;
    
    s.syscall('l', 1500 + speed * 5);
    s.syscall('r', 1500 - speed * 5);
    %sp = [sp speed];
    
    
    set(h1, 'XData', x, 'YData', yfilt);
    set(h2, 'XData', x, 'YData', sp);
    drawnow;
end

% for n = 1:1000
%     packet = queue.dequeue();
%     img = packet.data();
%     
%     % Convert from java BufferedImage to native MATLAB image
%     w=img.getWidth();
%     h=img.getHeight();
%     b = uint8(zeros([h,w,3]));
%     pixels = uint8(img.getData().getPixels(0,0,w,h,[]));
%     for i = 1 : h
%         base = (i-1)*w*3+1;
%         b(i,1:w,:) = deal(reshape(pixels(base:(base+3*w-1)),3,w)');
%     end
%     
%     image(b);
%     drawnow;
% end

%% Clean up

% Close the tcp stream
tcpstream.close();

% Empty the queue
queue.clear();

% Close the service client
client.close();
