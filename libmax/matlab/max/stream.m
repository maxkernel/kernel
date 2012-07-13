function stream_obj = stream(type, varargin)
    import java.net.InetAddress;
    import org.maxkernel.service.streams.TCPStream;
    import org.maxkernel.service.streams.UDPStream;
    
    switch lower(type)
        case 'tcp'
            % Requested service type is TCP, make a new tcp stream
            switch length(varargin)
                case 1
                    stream_obj = TCPStream(InetAddress.getByName(varargin{1}));
                case 2
                    stream_obj = TCPStream(InetAddress.getByName(varargin{1}), uint16(varargin{2}));
                otherwise
                    throw(MException('MaxKernel:StreamError', 'Invalid arguments to TCP stream'));
            end
            
        case 'udp'
            % Requested service type is UDP, make a new tcp stream
            switch length(varargin)
                case 1
                    stream_obj = UDPStream(InetAddress.getByName(varargin{1}));
                case 2
                    stream_obj = UDPStream(InetAddress.getByName(varargin{1}), uint16(varargin{2}));
                otherwise
                    throw(MException('MaxKernel:StreamError', 'Invalid arguments to UDP stream'));
            end
            
            
        otherwise
            throw(MException('MaxKernel:StreamError', 'Unknown stream type'));
    end
