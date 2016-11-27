--[[---------------------------------------------------------------------------
Functions for drawing custom user interfaces in Lua
]]

require("keys")
require("logger")

function inc(val, min, max)
    if val == max then return min end
    if val < min then return min end
    return val + 1
end

function dec(val, min, max)
    if val == min then return max end
    if val > max then return max end
    return val - 1
end

button = {}
button.__index = button

function button.create(caption, x, y, font, w, h)
    local self = {}
    setmetatable(self, utton)
    self.font = font
    if self.font == nil then
        self.font = FONT.MED_LARGE
    end
    self.caption = caption
    self.pad = 5
    self.left = x
    self.top = y
    self.width = w
    self.height = h
    self.border = COLOR.WHITE
    self.foreground = COLOR.WHITE
    self.background = COLOR.BLACK
    self.highlight = COLOR.BLUE
    self.disabled_color = COLOR.DARK_GRAY
    self.disabled_background = COLOR.gray(5)
    self.focused = false
    self.disabled = false
    if h == nil then
        self.height = self.font.height + self.pad * 2
    end
    if w == nil then
        self.width = self.font:width(self.caption) + self.pad * 2
    end
    return b
end

function button:draw()
    local bg = self.background
    local fg = self.foreground
    if self.disabled then fg = self.disabled_color end
    if self.focused then
        if self.disabled then bg = self.disabled_background
        else bg = self.highlight end
    end
    display.rect(self.left,self.top,self.width,self.height,self.border,bg)
    local x =  self.width / 2 - self.font:width(self.caption) / 2
    display.print(self.caption,self.left + x,self.top + self.pad,self.font,fg,bg)
end

function button:handle_key(k)
    if k == KEY.SET and self.focused and self.disabled == false then return self.caption end
end

scrollbar = {}
scrollbar.__index = scrollbar

function scrollbar.create(step, min, max, x, y, w, h)
    local self = {}
    setmetatable(self, scrollbar)
    self.step = step
    self.min = min
    self.max = max
    self.value = min
    self.top = y or 0
    self.left = x or (display.width - 2)
    self.foreground = COLOR.BLUE
    self.width = w or 2
    self.height = h or (display.height - y)
    if h == nil then self.height = display.height - y end
    return self
end

function scrollbar:draw()
    --update max automatically from 'table'
    if self.table ~= nil then self.max = #(self.table) end
    --don't draw if we are not needed
    if (self.max - self.min + 1) * self.step <= self.height then return end
    
    local total_height = (self.max - self.min + 1) * self.step
    local thumb_height = self.height * self.height / total_height
    local offset = (self.value - self.min) * self.step * self.height / total_height
    display.rect(self.left, self.top + offset, self.width, thumb_height, self.foreground, self.foreground)
end

function scrollbar:scroll_into_view(index)
    if (self.max - self.min + 1) * self.step <= self.height then return end

    if index < self.value then
        self.value = index
    elseif index + 1 >= self.value + self.height // self.step then
        self.value = index + 1 - self.height // self.step
    end
end

function scrollbar:up()
    self.value = dec(self.value,self.min,self.max)
end

function scrollbar:down()
    self.value = inc(self.value,self.min,self.max)
end

selector = {}
selector.__index = selector
function selector.create(title, items, format, width)
    local self = {}
    setmetatable(self, selector)
    self.index = 1
    self.title = title
    self.items = items
    self.format = format
    self.font = FONT.LARGE
    self.pad = 4
    self.visible_items = math.min(#items, (display.height - self.pad * 4) // self.font.height - 1)
    self.cancel = false
    self.width = width
    self.height = (self.visible_items + 1) * self.font.height + self.pad * 4
    self.left = display.width // 2 - self.width // 2
    self.top = display.height // 2 - self.height // 2
    self.scrollbar = scrollbar.create(self.font.height, 1, #items, self.left + self.width - 2, self.top + self.font.height + self.pad * 3, 2, self.height - self.font.height - self.pad * 4)
    return self
end

function selector:select()
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
    menu.block(false)
    keys:stop()
    if status == false then
        handle_error(error)
        return false
    else
        return not self.cancel
    end
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
                if (pos + self.font.height) > (self.top + self.height) then break end
            end
        end
        self.scrollbar:draw()
    end)
end

textbox = {}
textbox.__index = textbox
function textbox.create(value,x,y,w,font,h)
    local tb = {}
    setmetatable(tb,textbox)
    tb.font = font
    if tb.font == nil then
        tb.font = FONT.MED_LARGE
    end
    tb.min_char = 32
    tb.max_char = 127
    tb.value = value
    tb.pad = 5
    tb.left = x
    tb.top = y
    tb.width = w
    tb.height = h
    tb.border = COLOR.WHITE
    tb.foreground = COLOR.WHITE
    tb.background = COLOR.BLACK
    tb.focused_background = COLOR.BLUE
    tb.col = 1
    if h == nil then
        tb.height = tb.font.height + 10
    end
    return tb
end

function textbox:draw()
    local bg = self.background
    if self.focused then bg = self.focused_background end
    display.rect(self.left,self.top,self.width,self.height,self.border,bg)
    display.print(self.value,self.left + self.pad,self.top + self.pad,self.font,self.foreground,bg)
    local w = self.font:width(self.value:sub(1,self.col - 1))
    if self.col == 1 then w = 0 end
    local ch = self.value:sub(self.col,self.col)
    display.print(ch,self.left + w + self.pad,self.top + self.pad,self.font,self.background,self.foreground)
end

function textbox:handle_key(k)
    if k ==  KEY.RIGHT then
        self.col = inc(self.col,1,#(self.value) + 1)
    elseif k ==  KEY.LEFT then
        self.col = dec(self.col,1,#(self.value) + 1)
    elseif k == KEY.WHEEL_RIGHT then
        --mod char
        local l = self.value
        if self.col < #l then
            local ch = l:byte(self.col)
            ch = inc(ch,self.min_char,self.max_char)
            self.value = string.format("%s%s%s",l:sub(1,self.col - 1),string.char(ch),l:sub(self.col + 1))
        else
           self.value = l..string.char(self.min_char)
        end
    elseif k == KEY.WHEEL_LEFT then
        --mod char
        local l = self.value
        if self.col < #l then
            local ch = l:byte(self.col)
            ch = dec(ch,self.min_char,self.max_char)
            self.value = string.format("%s%s%s",l:sub(1,self.col - 1),string.char(ch),l:sub(self.col + 1))
        else
            self.value = l..string.char(self.max_char)
        end
    elseif k == KEY.PLAY then
        --insert
        local l = self.value
        self.value = string.format("%s %s",l:sub(1,self.col),l:sub(self.col + 1))
        self.col = self.col + 1
    elseif k == KEY.TRASH then
        --delete
        local l = self.value
        if self.col <= #l then
            self.value = string.format("%s%s",l:sub(1,self.col - 1),l:sub(self.col + 1))
        end
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
    local log = logger("LUA.ERR")
    log:write(error)
    log:close()
    keys:anykey()
end
