--[[
This gives you a way to execute Lua commands on your camera, remotely,
from a PC, using a wireless SD card. Communication is very hackish,
using a file where you write Lua commands; this module reads it in a loop
(polling), executes the commands it finds, and writes the result to another file.

Supported devices:
- Toshiba FlashAir W-03

Usage:
- copy this script (wifi_con.lua) to ML/SCRIPTS on your FlashAir card
- copy UPSCRIPT.LUA to the root of your card
- run wifi_console.lua on your PC
- try to fix the bugs (there are plenty)

TODO:
- port it to other WiFi cards
- use the native socket interface on WiFi-enabled cameras (6D, 70D)
]]--


-- write your commands here
cmd_file = "B:/CMD.LUA"

-- read this file to get script output
log_file = "B:/OUT.LOG"

-- base64 routine from:
-- Lua 5.1+ base64 v3.0 (c) 2009 by Alex Kloss <alexthkloss@web.de>
-- licensed under the terms of the LGPL2
-- base64_decodeoding
function base64_decode(data)
    -- character table string
    local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    data = string.gsub(data, '[^'..b..'=]', '')
    return (data:gsub('.', function(x)
        if (x == '=') then return '' end
        local r,f='',(b:find(x)-1)
        for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and '1' or '0') end
        return r;
    end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x)
        if (#x ~= 8) then return '' end
        local c=0
        for i=1,8 do c=c+(x:sub(i,i)=='1' and 2^(8-i) or 0) end
        return string.char(c)
    end))
end

-- intercept print
local old_print = print
local log_fout = nil
print = function(...)
    old_print(...);

    -- fixme: easier way to print to a file?
    local n = select("#",...)
    for i = 1,n do
        local v = tostring(select(i,...))
        log_fout:write(v)
        if i~=n then log_fout:write'\t' end
    end
    log_fout:write'\n'
end

local function print_results(...)
    -- This function takes care of nils at the end of results and such.
    if select('#', ...) > 1 then
        print(select(2, ...))
    end
end

function main()
    console.show()
    
    local old_code = ""

    -- if this file is not found, the upload script will not create it
    -- this is intentional, to bypass Canon's file caching from the camera
    old_print('Creating', cmd_file)
    local file = io.open(cmd_file, "w")
    file:write(string.rep(string.char(0), 512))
    file:close()
    
    old_print("WiFi Lua console at your service!")
    
    while true do
        local file = io.open(cmd_file, "r")
        if file then
            -- fixme: base64_decodeoding should be done when uploading the code
            local code = base64_decode(file:read("*all"));
            file:close()
            
            -- execute the code, if different
            -- fixme: you can't run the same line of code twice
            if code ~= old_code then
                msleep(300)
            
                -- read the file again, to be sure it's not in the middle of writing
                -- fixme: use a checksum instead of this hack
                file = io.open(cmd_file, "r")
                local code2 = base64_decode(file:read("*all"));
                file:close()

                if code == code2 then
                    -- looks OK
                    old_code = code
                    old_print("> " .. code)

                    -- trick from http://stackoverflow.com/questions/20410082/why-does-the-lua-repl-require-you-to-prepend-an-equal-sign-in-order-to-get-a-val
                    local f, err = load(code, '')
                    if err then
                        -- Maybe it's an expression.
                        f = load('return (' .. code .. ')', '')
                    end

                    -- save results, for sending them to PC
                    log_fout = io.open(log_file, "w")
                    
                    if f then
                        -- execute the code and capture results
                        print_results(pcall(f))
                    else
                        -- display the error
                        print_results(0,err)
                    end
                    
                    -- make sure the file isn't empty, by adding a newline
                    log_fout:write("\n")
                    
                    log_fout:close()
                end
            end
        end
        
        msleep(500)
    end
end

task.create(main)
