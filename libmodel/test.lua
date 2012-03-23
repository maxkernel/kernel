
log("Hello, World!")

t = loadmodule("libtest2.so")
t.config["cfg_param"] = 1
x = { t.cfg_param }

