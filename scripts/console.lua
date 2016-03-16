
require("keys")
require("keyboard")

console = {}
console.value = ""
console.lines = {"Welcome to the Lua Console"}

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
        local command = keyboard:show("",nil,nil,nil,#(self.lines) + 1,self.lines)
        if command ~= nil then
            local status,result = pcall(load,"return "..command)
            if status == false or result == nil then
                self.value = "syntax error"
            else
                status,self.value = pcall(result)
                self.value = tostring(self.value)
            end
            table.insert(self.lines,self.value)
        else break end
    end
    keys:stop()
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
