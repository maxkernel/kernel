classdef serviceclient
    properties(Access = private)
        obj_
    end
    methods
        function o = serviceclient()
            import org.maxkernel.service.ServiceClient;
            
            o.obj_ = ServiceClient;
        end
        function queue = begin(o, stream)
            import java.util.concurrent.LinkedBlockingQueue;
            import org.maxkernel.service.format.DoubleArrayFormat;
            import org.maxkernel.service.format.BufferedImageFormat;
            
            bqueue = LinkedBlockingQueue();
            fmt = char(stream.service_.format());

            switch lower(fmt)
                case 'doubles'
                    queue = DoubleArrayFormat(bqueue);
                case 'jpeg'
                    queue = BufferedImageFormat(bqueue);
                otherwise
                    throw(MException('MaxKernel:FormatError', sprintf('Unknown format: %s', fmt)));
            end

            o.obj_.begin(stream.obj_, bqueue);
        end
        function close(o)
            o.obj_.close();
        end
    end
end
