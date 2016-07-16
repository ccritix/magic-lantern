-- Intervalometer Ramping

require('config')
require("keys")
require("logger")
require("class")

option = class()
function option:__init(p)
    object.__init(self,p)
    self.scale = self.scale or 1/3
    self.offset = self.offset or 0
    self.starting_value = 0
end
function option:init_start()
    self.starting_value = self:get_value()
end
function option:set_value(value)
    self.source[self.property] = self.starting_value + self:convert(value)
end
function option:get_value()
    return self.source[self.property]
end
function option:convert(value)
    return value * self.scale + self.offset
end
function option:convert_back(value)
    return (value - self.offset - self.starting_value) / self.scale
end
function option:convert_absolute(value)
    return self.starting_value + self:convert(value)
end
function option:format(value)
    local v = self:convert(value)
    local sign = ""
    if v >= 0 then sign = "+" end
    return string.format("%s%s EV (%s APEX)", sign, tostring(v), tostring(self:convert_absolute(value)))
    --return string.format("%+.2f EV (%.2f APEX)", v, (self:convert_absolute(value)))
end

-- for Tv and Av, larger apex values are decreasing exposure
apex_option = class(option)
function apex_option:__init(p)
    option.__init(self,p)
end
function apex_option:set_value(value)
    self.source[self.property] = self.starting_value - self:convert(value)
end
function apex_option:convert_back(value)
    return (self.starting_value - (value - self.offset)) / self.scale
end
function apex_option:convert_absolute(value)
    return self.starting_value - self:convert(value)
end

wb_option = class(option)
function wb_option:format(value)
    return string.format("%d K", self:convert(value))
end
function wb_option:init_start() end

interval_option = class(option)
function interval_option:format(value)
    return string.format("%2d:%02d", value // 60, value % 60)
end
function interval_option:init_start() end

ramp = {}

ramp.options = 
{
    apex_option {
        name = "Shutter",
        source = camera.shutter,
        property = "apex",
        color = COLOR.BLUE,
        min = -5,
        max = 12,
    },
    apex_option {
        name = "Aperture",
        source = camera.aperture,
        property = "apex",
        color = COLOR.RED,
        min = camera.aperture.min.apex,
        max = camera.aperture.max.apex
    },
    option {
        name = "ISO",
        source = camera.iso,
        property = "apex",
        color = COLOR.GREEN1,
        min = 5,
        max = 11,
    },
    option {
        name = "EC",
        source = camera.ec,
        property = "value",
        color = COLOR.CYAN,
        min = -5,
        max = 5,
    },
    option {
        name = "Flash EC",
        source = camera.flash_ec,
        property = "value",
        color = COLOR.ORANGE,
        min = -5,
        max = 5,
    },
    wb_option {
        name = "WB",
        source = camera,
        property = "kelvin",
        color = COLOR.MAGENTA,
        scale = 100,
        offset = 5500,
    },
    interval_option {
        name = "Interval",
        source = interval,
        property = "time",
        color = COLOR.YELLOW,
        scale = 1
    },
}

ramp.selected_option = 1
ramp.x = 0
ramp.y = 0
ramp.xmin = 0
ramp.ymin = -15
ramp.xmax = 10
ramp.ymax = 15
ramp.font = FONT.MED

function linear_ramp(start_kf, end_kf, frame)
    return (frame - start_kf.frame) * (end_kf.value - start_kf.value) / (end_kf.frame - start_kf.frame) + start_kf.value;
end

initialized = false
function event.intervalometer(count)
    if ramp.menu.submenu["Enabled"].value then
        if count == 1 or not initialized then
            if not initialized then
                initialized = true
                ramp.log = logger("RAMP.LOG")
            end
            for k,opt in pairs(ramp.options) do
                opt:init_start()
            end
        end
        for k,opt in pairs(ramp.options) do
            if opt.keyframes and #(opt.keyframes) > 1 then
                local prev_kf
                for j,kf in pairs(opt.keyframes) do
                    if kf.frame == count then
                        ramp.log:writef("Ramping %s (%d): %s\n", opt.name, count, opt:format(kf.value))
                        opt:set_value(kf.value)
                    elseif kf.frame > count and prev_kf then
                        local value = linear_ramp(prev_kf, kf, count)
                        ramp.log:writef("Ramping %s (%d): %s\n", opt.name, count, opt:format(value))
                        opt:set_value(value)
                    end
                    prev_kf = kf
                end
            end
        end
    end
end

function ramp:edit()
    local status, error = xpcall(function()
        for k,v in pairs(self.options) do
            v:init_start()
        end
        self:main_loop()
    end, debug.traceback)
    if status == false then
        handle_error(error)
    end
    menu.block(false)
    keys:stop()
end

function ramp:main_loop()
    menu.block(true)
    self:draw()
    keys:start()
    local exit = false
    while not exit do
        if menu.visible == false then break end
        local key = keys:getkey()
        if key ~= nil then
            -- process all keys in the queue (until getkey() returns nil), then redraw
            while key ~= nil do
                exit = self:handle_key(key)
                if exit then break end
                key = keys:getkey()
            end
            self:draw()
            --don't yield long, see if there's more keys to process
            task.yield(1)
        else
            task.yield(100)
        end
    end
    keys:stop()
    if self.running == false then menu.block(false) end
end

function ramp:handle_key(k)
    if k == KEY.MENU then
        return true
    elseif k == KEY.INFO then
        self.selected_option = self.selected_option + 1
        if self.selected_option > #(self.options) then self.selected_option = 1 end
    elseif k == KEY.LEFT then
        if self.x > 0 then self.x = self.x - 1 end
    elseif k == KEY.RIGHT then
        self.x = self.x + 1
        if self.x > self.xmax then self.xmax = self.x end
    elseif k == KEY.UP then
        self.y = self.y + 1
        if self.y > self.ymax then self.ymax = self.y end
    elseif k == KEY.DOWN then
        self.y = self.y - 1
        if self.y < self.ymin then self.ymin = self.y end
    elseif k == KEY.SET then
        self:add_keyframe()
    end
    self:check_position()
    return false
end

function ramp:check_position()
    local opt = self.options[self.selected_option]
    if opt then
        if opt.min and opt.min > opt:convert_absolute(self.y) then
            self.y = opt:convert_back(opt.min)
        elseif opt.max and opt.max < opt:convert_absolute(self.y) then
            self.y = opt:convert_back(opt.max)
        end
    end
end

function ramp:add_keyframe()
    local opt = self.options[self.selected_option]
    if opt ~= nil then
        local kf = { frame = self.x, value = self.y }
        if opt.keyframes == nil then
            opt.keyframes = { kf }
        else
            local found = false
            for i,v in ipairs(opt.keyframes) do
                if self.x < v.frame then
                    table.insert(opt.keyframes,i,kf)
                    found = true
                    break
                elseif self.x == v.frame then
                    if v.value == self.y then
                        table.remove(opt.keyframes,i)
                    else
                        v.value = self.y
                    end
                    found = true
                    break
                end
            end
            if not found then
                table.insert(opt.keyframes,kf)
            end
        end
    end
end

function ramp:draw()
    display.draw(function()
        display.rect(0, 0, display.width, display.height, COLOR.BLACK, COLOR.BLACK)
        local pos = 1
        local max = 0
        local selected_color = COLOR.WHITE
        for i,v in ipairs(self.options) do
            if i == self.selected_option then
                display.print(v.name, 1, pos, self.font, COLOR.WHITE, v.color)
                selected_color = v.color
            else
                display.print(v.name, 1, pos, self.font, v.color, COLOR.BLACK)
            end
            max = math.max(max, self.font:width(v.name))
            pos = pos + self.font.height
        end
        max = max + 2
        display.line(max, 0, max, display.height, COLOR.GRAY)
        local tx = function(x) return max + (x - self.xmin) * (display.width - max) // (self.xmax - self.xmin) end
        local ty = function(y) return (self.ymax - y) * display.height // (self.ymax - self.ymin) end
        
        -- grid
        for x = self.xmin,self.xmax,1 do
            local gx = tx(x)
            display.line(gx, 0, gx, display.height, COLOR.DARK_GRAY)
        end
        
        -- x-axis
        display.line(tx(0), ty(0), tx(self.xmax), ty(0), COLOR.GRAY)
        
        -- graphs
        for i,v in ipairs(self.options) do
            if v.keyframes ~= nil then
                local pt1 = nil
                local pt2 = nil
                for i,p in ipairs(v.keyframes) do
                    pt2 = 
                    {
                        x = tx(p.frame),
                        y = ty(p.value)
                    }
                    display.circle(pt2.x, pt2.y, 8, v.color, v.color)
                    if pt1 ~= nil then
                        display.line(pt1.x, pt1.y, pt2.x, pt2.y, v.color)
                    end
                    pt1 = pt2
                end
            end
        end
        
        -- cursor
        local px = tx(self.x)
        local py = ty(self.y)
        display.rect(px - 8, py - 8, 16, 16, COLOR.WHITE, selected_color)
        local opt = self.options[self.selected_option]
        display.print(string.format("%s: %s", opt.name, opt:format(self.y)), max + 1, 1, self.font, COLOR.WHITE, COLOR.BLACK)
        display.print(string.format("frame: %s", self.x), max + 1, self.font.height + 1, self.font, COLOR.WHITE, COLOR.BLACK)
    end)
end

ramp.menu = menu.new
{
    name = "Ramping",
    parent = "Intervalometer",
    help = "Advanced intervalometer ramping",
    icon_type = ICON_TYPE.BOOL,
    submenu =
    {
        {
            name = "Enabled",
            max = 1,
            icon_type = ICON_TYPE.BOOL,
            help = "Enable advanced intervalometer ramping",
        },
        --[[ Not Implemented:
        {
            name = "Use Global Time",
            max = 1,
            icon_type = ICON_TYPE.BOOL,
            help = "Set keyframes by global time (time of day)",
        },
        {
            name = "External Source",
            max = 1,
            icon_type = ICON_TYPE.BOOL,
            help = "Use this module with an external intervalometer"
        },]]
        {
            name = "Edit Keyframes",
            select = function(this) task.create(function() ramp:edit() end) end,
            help = "View and edit keyframes",
        },
    }
}

config.create_from_menu(ramp.menu)


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
    local log = logger("RAMP.ERR")
    log:write(error)
    log:close()
    keys:anykey()
end