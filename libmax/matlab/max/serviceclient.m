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
            import org.maxkernel.service.queue.BooleanArrayServiceQueue;
            import org.maxkernel.service.queue.DoubleArrayServiceQueue;
            import org.maxkernel.service.queue.BufferedImageServiceQueue;
            
            bqueue = LinkedBlockingQueue();
            fmt = char(stream.service_.format());

            switch lower(fmt)
                case 'bools'
                    queue = BooleanArrayServiceQueue(bqueue);
                case 'doubles'
                    queue = DoubleArrayServiceQueue(bqueue);
                case 'jpeg'
                    queue = BufferedImageServiceQueue(bqueue);
                otherwise
                    throw(MException('MaxKernel:FormatError', sprintf('Unknown format: %s', fmt)));
            end

            o.obj_.begin(stream.obj_, queue);
        end
        function close(o)
            o.obj_.close();
        end
    end
end
