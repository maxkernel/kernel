classdef syscallclient
    properties(Access = private)
        obj_
    end
    methods
        function o = syscallclient(host, varargin)
            import java.net.InetAddress;
            import org.maxkernel.syscall.SyscallClient;
            
            switch length(varargin)
                case 0
                    o.obj_ = SyscallClient(InetAddress.getByName(host));
                case 1
                    o.obj_ = SyscallClient(InetAddress.getByName(host), uint16(varargin{1}));
                otherwise
                    throw(MException('MaxKernel:SyscallError', 'Invalid arguments to syscallclient constructor'));
            end
        end
        function sig = signature(o, name)
            sig = char(o.obj_.signature(name));
        end
        function ret = syscall(o, name, varargin)
            sig = o.signature(name);
            if isempty(sig)
                throw(MException('MaxKernel:SyscallError', 'Syscall does not exist'));
            end
            
            tok = regexp(sig, '^([A-Za-z]?):([A-Za-z]*)$', 'tokens');
            args = char(tok{:}(2))';
            
            array = javaArray('java.lang.Object', length(args));
            for i = 1:numel(args)
                switch args(i)
                    case 'v'
                    case 'b'
                        if varargin{i}
                            array(i) = java.lang.Boolean.TRUE;
                        else
                            array(i) = java.lang.Boolean.FALSE;
                        end
                    case 'i'
                        array(i) = java.lang.Integer(varargin{i});
                    case 'd'
                        array(i) = java.lang.Double(varargin{i});
                    case 'c'
                        array(i) = java.lang.Character(varargin{i});
                    case 's'
                        array(i) = java.lang.String(varargin{i});
                    otherwise
                        throw(MException('MaxKernel:SyscallError', 'Invalid syscall signature'));
                end
            end
            
            ret = o.obj_.syscall(name, array);
        end
        function close(o)
            o.obj_.close();
        end
    end
end