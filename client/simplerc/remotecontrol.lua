require("socket")
require("max")

local robot = max.connect()
if (robot == nil) then
	print("Error: could not connect to robot")
	os.exit(1)
end

local port = 10301
if (host == nil) then
	host = "*"
end

print("Starting UDP receiver...")
local udp = socket.udp()

print("Listening for client '" .. host .. "' on port " .. port)
assert(udp:setsockname("*", port))
assert(udp:setpeername(host, 0))
udp:settimeout(0.25)

print("Waiting for packets...");
while (1) do
	local dgram = udp:receive()
	
	if (dgram) then
	
		local thing = tonumber(string.sub(dgram, 1, 1))
		local value = tonumber(string.sub(dgram, 2))
		
		assert(thing)
		assert(value)
		
		if (thing == 0) then
			robot.motor(value)
		elseif (thing == 1) then
			robot.direction_set(value)
		elseif (thing == 2) then
			robot.turn(value)
		elseif (thing == 3) then
			robot.turnfront(value)
		elseif (thing == 4) then
			robot.turnrear(value);
		elseif (thing == 5) then
			--pan
		elseif (thing == 6) then
			--tilt
		elseif (thing == 7) then
			--ignore i'm alive command 
		else
			io.write("E")
		end
	else
		robot.motoroff()
		io.write(".")
	end
	io.flush()
end
