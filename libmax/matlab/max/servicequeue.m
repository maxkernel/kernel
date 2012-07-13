function queue_obj = servicequeue(client, service, stream)
    import java.util.concurrent.LinkedBlockingQueue;
    import org.maxkernel.service.format.DoubleArrayFormat;
    
    queue = LinkedBlockingQueue();
    fmt = char(service.format());
    
    switch fmt
        case 'doubles'
            queue_obj = DoubleArrayFormat(queue);
        otherwise
            throw(MException('MaxKernel:FormatError', sprintf('Unknown format: %s', fmt)));
    end
    
    client.begin(stream, queue);
