
log("Hello, World!")

t = loadmodule("libtest2.so")
t.config["cfg_param"] = 1

b = t.test_block(true, 9, 0.00000000000001)
route(b.output3, b.input1)

r = newrategroup("Rategroup 1", { b }, 10.33)
s = newsyscall("syscall_foo", { b.input1 }, b.output2, "The desc")

route(s.output, r.input)

