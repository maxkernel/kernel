classdef servicestream < handle
    properties(Access = private)
        services_
    end
    properties(GetAccess = 'public', SetAccess = 'private')
        obj_
        service_
    end
    methods
        function o = servicestream(type, varargin)
            import java.net.InetAddress;
            import org.maxkernel.service.streams.TCPStream;
            import org.maxkernel.service.streams.UDPStream;
            
            o.services_ = [];
            o.service_ = [];
            
            switch lower(type)
                case 'tcp'
                    % Requested service type is TCP, make a new tcp stream
                    switch numel(varargin)
                        case 1
                            o.obj_ = TCPStream(InetAddress.getByName(varargin{1}));
                        case 2
                            o.obj_ = TCPStream(InetAddress.getByName(varargin{1}), uint16(varargin{2}));
                        otherwise
                            throw(MException('MaxKernel:StreamError', 'Invalid arguments to TCP stream'));
                    end

                case 'udp'
                    % Requested service type is UDP, make a new tcp stream
                    switch numel(varargin)
                        case 1
                            o.obj_ = UDPStream(InetAddress.getByName(varargin{1}));
                        case 2
                            o.obj_ = UDPStream(InetAddress.getByName(varargin{1}), uint16(varargin{2}));
                        otherwise
                            throw(MException('MaxKernel:StreamError', 'Invalid arguments to UDP stream'));
                    end

                otherwise
                    throw(MException('MaxKernel:StreamError', 'Unknown stream type'));
            end
        end
        function list = services(o)
            o.services_ = [];
            
            itr = o.obj_.services().entrySet().iterator();
            while itr.hasNext()
                entry = itr.next().getValue();
                o.services_ = [o.services_; { char(entry.name()), char(entry.format()), char(entry.description()) }];
            end
            
            list = o.services_;
        end
        function subscribe(o, service)
            import org.maxkernel.service.Service;
            
            if isempty(o.services_)
                % Services has not been called
                o.services();
            end
            
            for s = o.services_'
                display(s);
                if strcmp(s(1), service)
                    % Found the service we want, subscribe
                    o.service_ = Service(s(1), s(2));
                    o.obj_.subscribe(o.service_);
                    return;
                end
            end
            
            throw(MException('MaxKernel:ServiceError', 'Service does not exist!'));
        end
        function close(o)
            o.obj_.close();
        end
    end
end
