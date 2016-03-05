
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
      {"#+="    ,".",",","?","!","'",    "DEL"},
      {"ABC",    "SPACE",   "Cancel",   "OK"  }
  },
  {
      {"[","]","{","}","#","%","^","*","+","="},
      { "_","\\","|","~","<",">","£","¥",'"'  },
      {"123"    ,".",",","?","!","'",    "DEL"},
      {"ABC",    "SPACE",   "Cancel",   "OK"  }
  }
}
keyboard.rows = 4
keyboard.cols = 10
keyboard.font = FONT.CANON
keyboard.border = COLOR.gray(75)
keyboard.background = COLOR.BLACK
keyboard.foreground = COLOR.WHITE
keyboard.highlight = COLOR.BLUE
keyboard.error_forground = COLOR.RED
keyboard.cursor_color = COLOR.ORANGE
keyboard.cell_width = display.width // 10
keyboard.cell_height = display.height // 6
keyboard.cell_pad_left = (keyboard.cell_width - keyboard.font:width("0")) // 2
keyboard.cell_pad_top = (keyboard.cell_height - keyboard.font.height) // 2
keyboard.layout_index = 1

function keyboard:show(default,maxlen,charset)
    self.value = default
    self.maxlen = maxlen
    self.charset = charset
    self.pos = #(self.value)
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
        self.pos = math.max(0,self.pos-1)
    elseif k == KEY.RIGHT then
        self.pos = math.min(#(self.value),self.pos+1)
    elseif k == KEY.UP then
        self.pos = #(self.value)
    elseif k == KEY.DOWN then
        self.pos = 0
    elseif k == KEY.SET then
        return false
    elseif k == KEY.MENU then
        self.value = nil
        return false
    elseif type(k) == "table" or type(k) == nil then
        local i = k.y * 6 // display.height
        if i >= 2 and i <=6 then
            self.row = i - 1
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

function keyboard:draw()
    display.draw(function()
        display.rect(0,0,display.width,display.height,self.border,self.background)
        display.print(self.value,20,20,self.font,self.foreground,self.background)
        local str = string.sub(self.value, 1, self.pos)
        local cur_pos = self.font:width(str) + 20
        display.rect(cur_pos,20,4,self.font.height,self.cursor_color,self.cursor_color)
        local current = self.layouts[self.layout_index]
        local y = display.height * 2 // 6
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
                local pad = w // 2 - self.font:width(c) // 2
                display.print(c,x2 + pad, y + self.cell_pad_top,self.font,fg,bg)
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