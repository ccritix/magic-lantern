--[[
    Remote Lua console for ML, via wifi card
    This file should be executed on the PC.
    On the camera, you should execute wifi_con.lua.
    On the FlashAir card, this script will invoke upscript.lua.
]]--

-- IP address of your FlashAir card
card_ip = "192.168.0.111"

-- read this file to get script output
log_file = "/OUT.LOG"

local function print_results(...)
    -- This function takes care of nils at the end of results and such.
    if select('#', ...) > 1 then
        print("out:", select(2, ...))
    end
end

function os.capture(cmd)
  local f = assert(io.popen(cmd, 'r'))
  local s = assert(f:read('*a'))
  f:close()
  return s
end

-- base64 routine from:
-- Lua 5.1+ base64 v3.0 (c) 2009 by Alex Kloss <alexthkloss@web.de>
-- licensed under the terms of the LGPL2

-- character table string
local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'

-- encoding
function enc(data)
    return ((data:gsub('.', function(x) 
        local r,b='',x:byte()
        for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and '1' or '0') end
        return r;
    end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
        if (#x < 6) then return '' end
        local c=0
        for i=1,6 do c=c+(x:sub(i,i)=='1' and 2^(6-i) or 0) end
        return b:sub(c+1,c+1)
    end)..({ '', '==', '=' })[#data%3+1])
end

function get_file(f)
    return os.capture("curl -s --max-time 5 http://"..card_ip.."/"..f)
end

function syntax_check(code)
    local f, err = load(code, '')
    if err then
        -- Maybe it's an expression.
        f, err = load('return (' .. code .. ')', '')
    end
    return err
end

function remote_exec(s)
    
    -- check the syntax locally first
    err = syntax_check(s)
    if err then
        print(err)
        return
    end
    
    -- upload the script to the camera
    result = os.capture("curl -s --max-time 5 http://"..card_ip.."/upscript.lua?"..enc(s))
    if result ~= "UPLOAD\nSUCCESS\n" then
        if result:len() > 0 then
            print("Error:", result)
        else
            print("Error: no response")
        end
        return
    end
    
    -- progress indicator to show that our script was uploaded
    io.write('...')
    io.flush()

    while true do
        -- give some time for the script to execute
        os.execute("sleep 1")
        
        result = get_file(log_file)
        if result:len() > 0 then

            -- try again, just to be sure
            os.execute("sleep 0.5")
            result2 = get_file(log_file)
            
            -- erase the progress indicator
            io.write(string.char(8), string.char(8), string.char(8), "   ", string.char(8), string.char(8), string.char(8))
            
            if result:len() > result2:len() then
                io.write(result)
                io.write(result2)
                print("(?!)")
            else
                -- this always ends with a newline
                if result2 ~= "\n" then
                    io.write(result2:sub(0,-2))
                end
            end
            break
        end
    end
end

repeat -- REPL
    io.write'> '
    io.stdout:flush()
    local s = io.read()
    if s == 'exit' or s == 'quit' then
        break
    elseif s == 'help' then
        print('Special commands: exit quit snap')
    elseif s == '' then
        -- empty string, don't bother sending it
    elseif s == 'snap' then
        -- screenshot
        remote_exec('display.screenshot("SNAP.PPM")')
        snap = get_file('SNAP.PPM')
        file = io.open("SNAP.PPM", "wb")
        file:write(snap)
        file:close()
        os.execute("eom SNAP.PPM &")
    else
        -- some code snippet to run on the camera
        remote_exec(s)
    end
until false
