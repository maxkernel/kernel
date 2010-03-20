
require("max")

m = max.connect("192.168.1.197")
print(m.info("info"))
--print("ID: " .. max.kernel_id())
--print("SYSCALLS: " .. max.syscall_list())

--print("EVAL: " .. max.math_eval(" pi / sin( 10 * 20 ) "))

--print(max.syscall_exists("max_model", "s:v")) 
--print(max.syscall_exists("bad_syscall", "i:i")) 

--if (max.module_exists("maxpod")) then
--	print("EXISTS!");
--else
--	print("DOESN'T EXIST!");
--end

--print("INFO: " .. max.info("info"))
--print("BAD INFO: " .. max.info("bad_syscall"))

