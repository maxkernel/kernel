
log("Hello, World!")

t = loadmodule("libtest2.so")
t.config["cfg_param"] = 1

b = t.test_block(true, 9, 0.00000000000001)
--route(b.output3, b.tinput1)

r = newrategroup("Rategroup 1", { b }, 10.33)
s = newsyscall("syscall_foo", { b.tinput1 }, b.toutput1, "The desc")

route(s.a2, r.rate)

