--[[---------------------------------------------------------------------------
File Dialog for Lua

This module is a lua script (@{filedlg.lua}), you need to explicitly load it
with `require('filedlg')`

@module filedialog
]]

filedialog = {}
filedialog.__index = filedialog

function filedialog.create()
    local fd = {}
    setmetatable(fd,filedialog)
    fd.font = FONT.MED_LARGE
    fd.top = 60
    fd.height = 380
    fd.left = 100
    fd.width = 520
    fd:createcontrols()
    return fd
end

function filedialog:createcontrols()
    self.save_box = textbox.create("",self.left,self.top + self.height - self.font.height - 10,self.width)
    --limit chars to those needed for filenames
    self.save_box.min_char = 46
    self.save_box.max_char = 95
    local w = self.width / 2
    self.ok_button = button.create("OK",self.left,self.top+self.height,self.font,w)
    self.cancel_button = button.create("Cancel",self.left + w,self.top+self.height,self.font,w)
    local title_height = 20 + FONT.LARGE.height
    self.scrollbar = scrollbar.create(self.font.height,0,1,self.left + self.width - 6,self.top + title_height, 2, self.height - title_height - self.save_box.height)
end

function filedialog:updatefiles()
    self.item_count = 0
    local status,items = xpcall(self.current.children,debug.traceback,self.current)
    if status == false then
        handle_error(items)
    end
    if status then 
        self.children = items
        self.item_count = #items
        table.sort(self.children, function(d1,d2) return d1.path < d2.path end)
    else 
        self.children = nil 
    end
    status,items = xpcall(self.current.files,debug.traceback,self.current)
    if status == false then
        handle_error(items)
    end
    if status then
        self.files = items
        self.item_count = self.item_count + #items
        table.sort(self.files)
    else 
        self.files = nil 
    end
    self.scrollbar.max = self.item_count
end

function filedialog:scroll_into_view()
    if self.selected < self.scrollbar.value then
        self.scrollbar.value = self.selected
    elseif self.selected >= self.scrollbar.value + (self.height - 20 - FONT.LARGE.height) / self.font.height - 3  then
        self.scrollbar.value = self.selected - ((self.height - 20 - FONT.LARGE.height) / self.font.height - 3)
    end
end

function filedialog:focus_next()
    if self.save_mode then
        self.focused_index = inc(self.focused_index,1,4)
    else
        self.focused_index = inc(self.focused_index,1,3)
    end
    self:update_focus()
end

function filedialog:update_focus()
    self.focused = (self.focused_index == 1)
    if self.save_mode then
        self.save_box.focused = (self.focused_index == 2)
        self.ok_button.focused = (self.focused_index == 3)
        self.cancel_button.focused = (self.focused_index == 4)
    else
        self.ok_button.focused = (self.focused_index == 2)
        self.cancel_button.focused = (self.focused_index == 3)
    end
end

function filedialog:handle_key(k)
    if k == KEY.Q then
        self:focus_next()
    elseif self.save_mode and self.focused_index == 2 then
        return self.save_box:handle_key(k)
    elseif self.focused_index == 2 then
        return self.ok_button:handle_key(k)
    elseif self.save_mode and self.focused_index == 3 then
        return self.ok_button:handle_key(k)
    elseif self.focused_index == 3 then
        return self.cancel_button:handle_key(k)
    elseif self.focused_index == 4 then
        return self.cancel_button:handle_key(k)
    elseif k == KEY.UP or k == KEY.WHEEL_UP then
        self.selected = dec(self.selected,0,self.item_count)
        self:scroll_into_view()
    elseif k == KEY.DOWN or k == KEY.WHEEL_DOWN then 
        self.selected = inc(self.selected,0,self.item_count)
        self:scroll_into_view()
    elseif k == KEY.SET then
        if self.selected == 0 then
            if self.current.parent ~= nil then
                self.current = self.current.parent
                self.selected = 1
                self.scrollbar.value = 0
                self:updatefiles()
            end
        elseif self.is_dir_selected and self.selected_value ~= nil then
            self.current = self.selected_value
            self.selected = 1
            self.scrollbar.value = 0
            self:updatefiles()
        elseif self.save_mode then
            local found = self.current.path:find("/[^/]+$")
            self.save_box.value = self.current.path:sub(found)
        else
            return self.selected_value
        end
    end
end

function filedialog:save(default_name)
    self.save_mode = true
    if default_name ~= nil then
        self.save_box.value = default_name
    end
    if self.save_box.value == nil then
        self.save_box.value = "UNTITLED"
    end
    return self:show()
end

function filedialog:open()
    self.save_mode = false
    return self:show()
end

function filedialog:show()
    if self.current == nil then
        self.current = dryos.directory("ML/SCRIPTS")
        self.selected = 1
        self.scrollbar.value = 0
    end
    self.focused_index = 1
    self:update_focus()
    local w = self.width/2
    self:updatefiles()
    self:draw()
    local started = keys:start()
    while true do
        local key = keys:getkey()
        if key ~= nil then
            -- process all keys in the queue (until getkey() returns nil), then redraw
            while key ~= nil do
                local result = self:handle_key(key)
                if result == "Cancel" then 
                    if started then keys:stop() end
                    return nil
                elseif result == "OK" then
                    if started then keys:stop() end
                    if self.save_mode then return self.current.path..self.save_box.value
                    else return self.selected_value end
                elseif result ~= nil then
                    if started then keys:stop() end
                    return result
                end
                key = keys:getkey()
            end
            self:draw()
        end
        task.yield(100)
    end
end

function filedialog:draw()
    display.draw(function()
        self:draw_main()
        self.scrollbar:draw()
        if self.save_mode then 
            self.save_box:draw()
        else
            self.ok_button.disabled = self.is_dir_selected
        end
        self.ok_button:draw()
        self.cancel_button:draw()
    end)
end

function filedialog:draw_main()
    display.rect(self.left, self.top, self.width, self.height, COLOR.WHITE, COLOR.BLACK)
    display.rect(self.left, self.top, self.width, 20 + FONT.LARGE.height, COLOR.WHITE, COLOR.gray(10))
    if self.save_mode then
        display.print(string.format("Save | %s",self.current.path), self.left + 10, self.top + 10, FONT.LARGE,COLOR.WHITE,COLOR.gray(10))
    else
        display.print(string.format("Open | %s",self.current.path), self.left + 10, self.top + 10, FONT.LARGE,COLOR.WHITE,COLOR.gray(10))
    end
    local pos = self.top + 20 + FONT.LARGE.height
    display.line(self.left, pos, self.width - self.left, pos, COLOR.WHITE)
    local x = self.left + 10
    local r = self.left + self.width
    pos = pos + 10
    local dir_count = #(self.children)
    local status,items
    local sel_color = COLOR.DARK_GRAY
    if self.focused then sel_color = COLOR.BLUE end
    self.is_dir_selected = false
    if self.current.exists ~= true then return end
    if self.scrollbar.value == 0 then
        if self.selected == 0 then
            display.rect(self.left + 1,pos,self.width - 2,self.font.height,sel_color,sel_color)
            display.print("..", x, pos, self.font, COLOR.WHITE, sel_color)
        else
            display.print("..", x, pos, self.font)
        end
        pos = pos + self.font.height
    end
    if self.children ~= nil then
        for i,v in ipairs(self.children) do
            if i >= self.scrollbar.value then
                if i == self.selected then
                    self.is_dir_selected = true
                    self.selected_value = v
                    display.rect(self.left + 1,pos,self.width - 2,self.font.height,sel_color,sel_color)
                    display.print(v.path, x, pos, self.font, COLOR.WHITE, sel_color)
                else
                    display.print(v.path, x, pos, self.font)
                end
                pos = pos + self.font.height
                if (pos + self.font.height) > (self.top + self.height) then return end
            end
        end
    end
    if self.files ~= nil then
        for i,v in ipairs(self.files) do
            if dir_count + i >= self.scrollbar.value then
                if dir_count + i == self.selected then
                    self.selected_value = v
                    display.rect(self.left + 1,pos,self.width - 2,self.font.height,sel_color,sel_color)
                    display.print(v, x, pos, self.font, COLOR.WHITE, sel_color)
                elseif self.save_mode then
                    display.print(v, x, pos, self.font, COLOR.GRAY, COLOR.BLACK)
                else
                    display.print(v, x, pos, self.font)
                end
                pos = pos + self.font.height
                if (pos + self.font.height) > (self.top + self.height) then return end
            end
        end
    end
end