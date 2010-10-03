signal = require "signal"

-- signal.signal("SIGINT")

signal.signal("SIGINT", function() 
    io.stdout:write("sigint\n") 
    os.exit(1)
end); 

-- while true do end
io.stdin:read()
