
require("keys")
require("keyboard")
require("logger")

shell = {}
shell.value = ""
shell.lines = {"Welcome to the Lua Shell",""}
shell.history = {}
shell.history_index = 0

function shell:init()
    self.logger = logger("LUASHELL.LOG",self)
    --intercept the builtin print
    print = function(...)
        for k,v in pairs({...}) do
            self.logger:serialize(v)
        end
    end
end

function shell:write(str)
    local lines = logger.tolines(str)
    self.lines[#(self.lines)] = self.lines[#(self.lines)]..lines[1]
    for i=2,#lines,1 do
        table.insert(self.lines,lines[i])
    end
end

function shell:run()
    local status, error = xpcall(function()
        self:init()
        self:main_loop()
    end, debug.traceback)
    if status == false then
        handle_error(error)
    end
    menu.block(false)
    keys:stop()
    self.logger:close()
end

function shell:main_loop()
    menu.block(true)
    keyboard.font = FONT.MONO_20
    while true do
        if menu.visible == false then break end
        local command = keyboard:show("",nil,function(k,w) self:handle_keyboard_up(k,w) end,function(k,w) self:handle_keyboard_down(k,w) end,#(self.lines),self.lines)
        if command ~= nil then
            self.logger:writef(">%s\n",command)
            local status,result = pcall(load, command)
            if status == false or result == nil then
                self.value = "syntax error"
            else
                status,self.value = xpcall(result, debug.traceback)
            end
            table.insert(self.history,command)
            self.history_index = #(self.history) + 1
            if self.value ~= nil then
                self.logger:serialize(self.value)
            end
            self.value = tostring(self.value)
            if self.lines[#(self.lines)] ~= "" then table.insert(self.lines,"") end
        else break end
    end
    keys:stop()
end

function shell:handle_keyboard_up(k,w)
    if not w then
        if self.history_index > 0 then self.history_index = self.history_index - 1 end
        k.value = self.history[self.history_index]
        if k.value == nil then k.value = "" end
        k.pos = #(k.value)
    end
end

function shell:handle_keyboard_down(k,w)
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
    local log = logger("SHELL.ERR")
    log:write(error)
    log:close()
    keys:anykey()
end

shell:run()
