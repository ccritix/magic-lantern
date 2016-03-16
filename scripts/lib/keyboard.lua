
keyboard = {}
keyboard.layouts = 
{
  {
      {"Q","W","E","R","T","Y","U","I","O","P"},
      {  "A","S","D","F","G","H","J","K","L"  },
      {"abc","Z","X","C","V","B","N","M","DEL"},
      {"123",    "SPACE",   "Cancel",   "OK"  }
  },
  {
      {"q","w","e","r","t","y","u","i","o","p"},
      {  "a","s","d","f","g","h","j","k","l"  },
      {"ABC","z","x","c","v","b","n","m","DEL"},
      {"123",    "SPACE",   "Cancel",   "OK"  }
  },
  {
      {"1","2","3","4","5","6","7","8","9","0"},
      {"-","/",":",";","(",")","$","&","@",'"'},
      {"#+=","\x7F",".",",","?","!","'","\x8B","DEL"},
      {"ABC",    "SPACE",   "Cancel",   "OK"  }
  },
  {
      {"[","]","{","}","#","%","^","*","+","="},
      { "_","\\","|","~","<",">","\x80","\x81","\x82"},
      {"123","\x83","\x84","\x85","\x86","\x87","\x88","\x89","DEL"},
      {"ABC",    "SPACE",   "Cancel",   "OK"  }
  }
}
keyboard.rows = 4
keyboard.cols = 10
keyboard.font = FONT.LARGE
keyboard.key_font = FONT.LARGE
keyboard.border = COLOR.gray(75)
keyboard.background = COLOR.BLACK
keyboard.foreground = COLOR.WHITE
keyboard.highlight = COLOR.BLUE
keyboard.error_forground = COLOR.RED
keyboard.cursor_color = COLOR.ORANGE
keyboard.grey_text = COLOR.gray(50)
keyboard.cell_width = display.width // 10
keyboard.cell_height = display.height // 8
keyboard.layout_index = 1

function keyboard:show(default,maxlen,up,down,line,lines)
    self.value = default
    self.maxlen = maxlen
    if up ~= nil then assert(type(up) == "function", "expected function, parameter 'down'") end
    if down ~= nil then assert(type(down) == "function", "expected function, parameter 'down'") end
    self.up = up
    self.down = down
    self.line = line
    self.lines = lines
    if self.line == nil or self.lines == nil then
        self.line = 1
        self.lines = { self.value }
    end
    self.pos = #(self.value)
    self.cell_pad_left = (self.cell_width - self.key_font:width("0")) // 2
    self.cell_pad_top = (self.cell_height - self.key_font.height) // 2
    self:update_lines()
    self:run()
    return self.value
end

function keyboard:run()
    self.finished = false
    self.current = self.upper
    local keys_started = keys:start()
    local status, error = xpcall(function()
        self:main_loop()
    end, debug.traceback)
    if status == false then
        keyboard_handle_error(error)
    end
    if keys_started then keys:stop() end
end

function keyboard:main_loop()
    self:draw()
    while true do
        if menu.visible == false or self.finished then break end
        local key = keys:getkey()
        if key ~= nil then
            if self:handle_key(key) == false then
                break
            end
            self:draw()
            --don't yield long, see if there's more keys to process
            task.yield(1)
        else
            task.yield(100)
        end
    end
end

function keyboard:handle_key(k)
    if k == KEY.LEFT then
        if self.up ~= nil and self.pos == 0 then 
            self:up(true)
            self:update_lines()
        else self.pos = math.max(0,self.pos-1) end
    elseif k == KEY.RIGHT then
        if self.down ~= nil and self.pos == #(self.value) then 
            self:down(true)
            self:update_lines()
        else self.pos = math.min(#(self.value),self.pos+1) end
    elseif k == KEY.UP then
        if self.up ~= nil then 
            self:up(false)
            self:update_lines()
        else self.pos = #(self.value) end
    elseif k == KEY.DOWN then
        if self.up ~= nil then 
            self:down(false)
            self:update_lines()
        else self.pos = 0 end
    elseif k == KEY.SET then
        return false
    elseif k == KEY.MENU then
        self.value = nil
        return false
    elseif type(k) == "table" then
        local i = k.y * 8 // display.height
        if i >= 4 and i <=8 then
            self.row = i - 3
            local current = self.layouts[self.layout_index]
            local row = current[self.row]
            local cols = #row
            if self.row == 4 then
                self.col = k.x * cols // display.width + 1
            else
                local pad = self.cell_width * (self.cols - cols) // 2
                self.col = (k.x - pad) // self.cell_width + 1
            end
            if self.col < 1 then self.col = 1
            elseif self.col > cols then self.col = cols end
            
            if k.type == 0 then
                local c = row[self.col]
                local len = #(self.value)
                if c == "Cancel" then self.value = nil return false
                elseif c == "OK" then return false
                elseif c == "DEL" then
                    if len > 0 and self.pos > 0 then
                        if self.pos == len then self.value = string.sub(self.value,1,len - 1)
                        else self.value = string.sub(self.value,1,self.pos - 1)..string.sub(self.value,self.pos+1,len) end
                        self.pos = self.pos - 1
                    end
                elseif c == "ABC" then
                    self.layout_index = 1
                    self.layouts[3][4][1] = "ABC"
                    self.layouts[4][4][1] = "ABC"
                elseif c == "abc" then
                    self.layout_index = 2
                    self.layouts[3][4][1] = "abc"
                    self.layouts[4][4][1] = "abc"
                elseif c == "123" then
                    self.layout_index = 3
                elseif c == "#+=" then
                    self.layout_index = 4
                else
                    if c == "SPACE" then c = " " end
                    if self.pos == 0 then self.value = c..self.value
                    elseif self.pos == len then self.value = self.value..c
                    else self.value = string.sub(self.value,1,self.pos)..c..string.sub(self.value,self.pos+1,len) end
                    self.pos = self.pos + 1
                end
            end
        else
            local len = #(self.value)
            local guess = (k.x - 20) // self.font:width("0")
            if guess > len then
                guess = len
            elseif guess < 0 then
                guess = 0
            else
                local cur = self.font:width(string.sub(self.value,1,guess)) + 20
                if k.x > cur then
                    --start searching forward
                    while k.x > cur do
                        local last = cur
                        if guess >= len then break end
                        cur = self.font:width(string.sub(self.value,1,guess+1)) + 20
                        if k.x < cur then
                            if (cur - k.x) < (k.x - last) then
                                guess = guess + 1
                            end
                            break
                        end
                        guess = guess + 1
                    end
                else
                    --start searching backward
                    while k.x < cur do
                        local last = cur
                        if guess < 1 then break end
                        cur = self.font:width(string.sub(self.value,1,guess-1)) + 20
                        if k.x > cur then
                            if (k.x - cur) < (last - k.x) then
                                guess = guess - 1
                            end
                            break
                        end
                        guess = guess - 1
                    end
                
                end
            end
            self.pos = guess
        end
    end
    return true
end

function keyboard:update_lines()
    local max_lines = (display.height // 2 - 2) // self.font.height - 1
    local total_lines_before = self.line - 1
    local total_lines_after = #(self.lines) - self.line
    self.lines_before = max_lines // 2
    self.lines_after = max_lines - self.lines_before
    if total_lines_before < self.lines_before then 
        self.lines_after = math.min(self.lines_after + self.lines_before - total_lines_before,total_lines_after)
        self.lines_before = total_lines_before
    elseif total_lines_after < self.lines_after then 
        self.lines_before = math.min(self.lines_before + self.lines_after - total_lines_after,total_lines_before)
        self.lines_after = total_lines_after
    end
    self.text_top = (display.height // 2 - (max_lines + 1) * self.font.height) // 2
end

function keyboard:draw()
    display.draw(function()
        display.rect(0,0,display.width,display.height,self.border,self.background)
        local y = self.text_top
        for i=-self.lines_before,-1,1 do
            display.print(self.lines[self.line + i],10,y,self.font,self.grey_text,self.background)
            y = y + self.font.height
        end
        display.print(self.value,10,y,self.font,self.foreground,self.background)
        local str = string.sub(self.value, 1, self.pos)
        local cur_pos = self.font:width(str) + 10
        display.rect(cur_pos,y,4,self.font.height,self.cursor_color,self.cursor_color)
        for i=1,self.lines_after,1 do
            y = y + self.font.height
            display.print(self.lines[self.line + i],10,y,self.font,self.grey_text,self.background)
        end
        
        local current = self.layouts[self.layout_index]
        y = display.height * 4 // 8
        local x = 0
        for i=1,4,1 do
            local row = current[i]
            local cols = #row
            local pad = self.cell_width * (self.cols - cols) // 2
            x = pad
            for j=1,cols,1 do
                local c = row[j]
                local bg = self.background
                local fg = self.foreground
                if i == self.row and j == self.col then bg = self.highlight end
                local x2 = x
                local w = self.cell_width
                if i == 3 then
                    --first and last items on row 3 should take the extra space
                    if j == 1 then 
                        x2 = 0
                        w = w + pad
                    elseif j == cols then
                        w = w + pad
                    end
                elseif i == 4 then
                    --space row 4 evenly
                    x2 = (display.width * (j - 1)) // cols
                    w = display.width // cols
                end
                display.rect(x2,y,w,self.cell_height,self.border,bg)
                local pad = w // 2 - self.key_font:width(c) // 2
                display.print(c,x2 + pad, y + self.cell_pad_top,self.key_font,fg,bg)
                x = x + self.cell_width
            end
            y = y + self.cell_height
        end
    end)
end

function keyboard_handle_error(error)
    if error == nil then error = "Unknown Error!\n" end
    local f = FONT.MONO_12
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