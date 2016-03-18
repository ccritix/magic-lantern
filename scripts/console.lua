
require("keys")
require("keyboard")

ml_console = console

console = {}
console.value = ""
console.lines = {"Welcome to the Lua Console"}
console.show = ml_console.show
console.hide = ml_console.hide
console.history = {}
console.history_index = 0

lua_print = print
function print(...)
    local str = ""
    for k,v in pairs({...}) do
        console:write_data(v)
    end
end

function console.write(str)
    console:write_data(str)
end

function console:write_data(data)
    local str = self.serialize("", data)
    local p1 = 1
    local p2 = 1
    local len = #str
    while p2 <= len do
        local c = string.sub(str,p2,p2)
        if c == "\r" or c == "\n" then
            if p1 == p2 then table.insert(self.lines,"")
            else table.insert(self.lines, string.sub(str,p1,p2-1)) end
            if p2 == len then break end
            p2 = p2 + 1
            if c == "\r" and string.sub(str,p2,p2) == "\n" then
                p2 = p2 + 1
            end
            p1 = p2
        else
            p2 = p2 + 1
        end
    end
    if p1 == p2 then table.insert(self.lines,"")
    else table.insert(self.lines, string.sub(str,p1,p2-1)) end
    --don't get too big
    while #(self.lines) > 50 do
        table.remove(self.lines,1)
    end
    self:write_log(str)
    ml_console.write(str)
    return str
end

function console:write_log(str)
    local log = io.open("LUASHELL.LOG", "a")
    log:write(str)
    log:write("\n")
    log:close()
end

function console.serialize(str,o,l)
    if type(o) == "string" then
        str = str..o
    elseif type(o) == "table" then
        if l == nil then l = 1 end
        str = str.."table:\n"
        for k,v in pairs(o) do
            for i=1,l,1 do str = str.."  " end
            str = console.serialize(str,k)
            str = str.." = "
            str = console.serialize(str,v,l+1)
            str = str.."\n"
        end
    else
        str = str..tostring(o)
    end
    return str
end

console.mlmenu = menu.new
{
    name = "Console",
    select = function(this)
        task.create(function() console:run() end)
    end,
    update = function(this) return console.value end
}

function console:run()
    local status, error = xpcall(function()
        menu.block(true)
        self:main_loop()
    end, debug.traceback)
    if status == false then
        handle_error(error)
    end
    menu.block(false)
    keys:stop()
end

function console:main_loop()
    menu.block(true)
    keyboard.font = FONT.MONO_20
    while true do
        if menu.visible == false then break end
        local command = keyboard:show("",nil,function(k,w) self:handle_keyboard_up(k,w) end,function(k,w) self:handle_keyboard_down(k,w) end,#(self.lines) + 1,self.lines)
        if command ~= nil then
            self:write_data(">"..command)
            local status,result = pcall(load, command)
            if status == false or result == nil then
                self.value = "syntax error"
            else
                status,self.value = xpcall(result, debug.traceback)
            end
            table.insert(self.history,command)
            self.history_index = #(self.history) + 1
            if self.value ~= nil then
                self:write_data(self.value)
            end
            self.value = tostring(self.value)
        else break end
    end
    keys:stop()
end

function console:handle_keyboard_up(k,w)
    if not w then
        if self.history_index > 0 then self.history_index = self.history_index - 1 end
        k.value = self.history[self.history_index]
        if k.value == nil then k.value = "" end
        k.pos = #(k.value)
    end
end

function console:handle_keyboard_down(k,w)
    if not w then
        if self.history_index <= #(self.history) then self.history_index = self.history_index + 1 end
        k.value = self.history[self.history_index]
        if k.value == nil then k.value = "" end
        k.pos = #(k.value)
    end
end

function handle_error(error)
    if error == nil then error = "Unknown Error!\n" end
    local f = FONT.MONO_20
    print(error)
    display.rect(0,0,display.width,display.height,COLOR.RED,COLOR.BLACK)
    local pos = 10
    for line in error:gmatch("[^\r\n]+") do
        local clipped = display.print(line,10,pos,f)
        while clipped ~= nil do
            pos = pos + f.height
            clipped = display.print(clipped,10,pos,f)
        end
        pos = pos + f.height
    end
    keys:anykey()
end
