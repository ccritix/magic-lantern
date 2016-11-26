--[[---------------------------------------------------------------------------
Functions for drawing custom user interfaces in Lua
]]

require("keys")
require("logger")

function inc(val,min,max)
    if val == max then return min end
    if val < min then return min end
    return val + 1
end

function dec(val,min,max)
    if val == min then return max end
    if val > max then return max end
    return val - 1
end

scrollbar = {}
scrollbar.__index = scrollbar

function scrollbar.create(step,min,max,x,y,w,h)
    local sb = {}
    setmetatable(sb,scrollbar)
    sb.step = step
    sb.min = min
    sb.max = max
    sb.value = min
    sb.top = y
    sb.left = x
    sb.foreground = COLOR.BLUE
    sb.width = w
    sb.height = h
    if h == nil then sb.height = display.height - y end
    return sb
end

function scrollbar:draw()
    --don't draw if we are not needed
    if (self.max - self.min + 1) * self.step <= self.height then return end
    
    local total_height = (self.max - self.min + 1) * self.step
    local thumb_height = self.height * self.height / total_height
    local offset = (self.value - self.min) * self.step * self.height / total_height
    display.rect(self.left,self.top + offset,self.width,thumb_height,self.foreground,self.foreground)
end

function scrollbar:scroll_into_view(index)
    if (self.max - self.min + 1) * self.step <= self.height then return end

    if index < self.value then
        self.value = index
    elseif index + 1 >= self.value + self.height // self.step then
        self.value = index + 1 - self.height // self.step
    end
end

selector = {}
selector.__index = selector
function selector.create(title, items, format, width)
    local s = {}
    setmetatable(s, selector)
    s.index = 1
    s.title = title
    s.items = items
    s.format = format
    s.font = FONT.MED_LARGE
    s.pad = 4
    s.visible_items = math.min(#items, (display.height - s.pad * 4) // s.font.height - 1)
    s.cancel = false
    s.width = width
    s.height = (s.visible_items + 1) * s.font.height + s.pad * 4
    s.left = display.width // 2 - s.width // 2
    s.top = display.height // 2 - s.height // 2
    s.scrollbar = scrollbar.create(s.font.height, 1, #items, s.left + s.width - 2, s.top + s.font.height + s.pad * 3, 2, s.height - s.font.height - s.pad * 4)
    return s
end

function selector:run()
    local status, error = xpcall(function()
        keys:start()
        menu.block(true)
        self:draw()
        while true do
            local key = keys:getkey()
            if key ~= nil then
                if not self:handle_key(key) then break end
                self:draw()
            end
            task.yield(100)
            if self.cancel then break end
        end
    end, debug.traceback)
    if status == false then
        handle_error(error)
    end
    menu.block(false)
    keys:stop()
end

function selector:handle_key(key)
    if key == KEY.UP or key == KEY.WHEEL_UP or key == KEY.RIGHT or key == KEY.WHEEL_RIGHT then
        self.index = dec(self.index, 1, #self.items)
    elseif key == KEY.DOWN or key == KEY.WHEEL_DOWN or key == KEY.LEFT or key == KEY.WHEEL_LEFT then
        self.index = inc(self.index, 1, #self.items)
    elseif key == KEY.SET then
        return false
    else
        self.cancel = true
        return false
    end
    self.scrollbar:scroll_into_view(self.index)
    return true
end

function selector:draw()
    display.draw(function()
        local pos = self.top
        local x = self.left + self.pad
        display.rect(self.left, pos, self.width, self.height, COLOR.WHITE, COLOR.BLACK)
        pos = pos + self.pad
        display.print(self.title, self.left + self.pad, pos, self.font, COLOR.WHITE, COLOR.BLACK)
        pos = pos + self.font.height + self.pad
        display.line(self.left, pos, self.left + self.width, pos, COLOR.WHITE)
        pos = pos + self.pad

        for i,v in ipairs(self.items) do
            if i >= self.scrollbar.value then
                local text = v
                if self.format ~= nil then
                    text = self.format(v)
                end
                if i == self.index then
                    display.rect(self.left + 1, pos, self.width - 2, self.font.height, COLOR.BLUE, COLOR.BLUE)
                    display.print(text, x, pos, self.font, COLOR.WHITE, COLOR.BLUE, self.width - self.pad * 2)
                else
                    display.print(text, x, pos, self.font, COLOR.WHITE, COLOR.BLACK, self.width - self.pad * 2)
                end
                pos = pos + self.font.height
                if (pos + self.font.height) > (self.top + self.height) then return end
            end
        end
        self.scrollbar:draw()
    end)
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
    local log = logger("LUA.ERR")
    log:write(error)
    log:close()
    keys:anykey()
end