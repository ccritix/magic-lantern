-- Upload a script received via HTTP request, encoded as base64,
-- to this file:

print('UPLOAD')

cmd_file = "/CMD.LUA"

-- couldn't get base64 decoding to run on the FlashAir card
-- first time I run the script, it works
-- next time it locks up the web server without any error message
-- so... we'll decode it from ML

script = arg[1]

if script:len() > 512 then
    print("Script too big.");
    return;
end

-- make sure the file size is 1 full sector (fill with zeros)
-- reason: Canon code caches the file list, including size
-- so we use a single sector, to keep things simple
script = script .. string.rep(string.char(0), 512 - #script)

-- note: overwriting the file from Lua does not change the file position
-- on the card (at least that's how it worked in my tests...)
-- credits: eduperez for the hint

-- so, make sure the file exists first
file = io.open(cmd_file, "r")
if not file then
    print("Output file does not exist.")
    return
end
file:close()

-- overwrite the file
file = io.open(cmd_file, "w")
if not file then
    print("Write error:", cmd_file)
    return
end

file:write(script)
file:close()
print("SUCCESS")
