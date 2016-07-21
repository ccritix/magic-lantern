-- Intervalometer Ramping

require('config')
require("keys")
require("logger")
require("class")

available_colors = 
{
    COLOR.GREEN1,
    COLOR.CYAN,
    COLOR.RED,
    COLOR.BLUE,
    COLOR.GREEN2,
    COLOR.DARK_RED,
    COLOR.MAGENTA,
    COLOR.YELLOW,
    COLOR.ORANGE,
    COLOR.DARK_CYAN1_MOD,
    COLOR.GRAY,
    COLOR.DARK_GREEN1_MOD,
    COLOR.LIGHT_BLUE,
    COLOR.DARK_ORANGE_MOD,
    COLOR.DARK_CYAN2_MOD,
    COLOR.DARK_GREEN2_MOD,
    COLOR.LIGHT_GRAY,
}
current_color = -1
function next_color()
    current_color = (current_color + 1) % (#available_colors)
    return available_colors[current_color + 1]
end

option = class()
function option:init(...)
    object.init(self,...)
    self.scale = self.scale or 1/3
    self.offset = self.offset or 0
    self.starting_value = 0
    self.color = next_color()
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
function apex_option:set_value(value)
    self.source[self.property] = self.starting_value - self:convert(value)
end
function apex_option:convert_back(value)
    return (self.starting_value - (value - self.offset)) / self.scale
end
function apex_option:convert_absolute(value)
    return self.starting_value - self:convert(value)
end

focus_option = class(option)
function focus_option:set_value(value)
    local delta = self:convert(value) - self.starting_value
    if lens.focus(delta, 1) then
        self.starting_value = self:convert(value)
    end
end
function focus_option:get_value()
    return self.starting_value
end
function focus_option:format(value)
    return string.format("%s steps", tostring(self:convert(value)))
end
function focus_option:init_start() 
    self.starting_value = 0
end

wb_option = class(option)
function wb_option:format(value)
    return string.format("%s K", tostring(self:convert(value)))
end
function wb_option:init_start() end

interval_option = class(option)
function interval_option:format(value)
    return string.format("%2d:%02d", value // 60, value % 60)
end
function interval_option:init_start() end

menu_option = class(option)
function menu_option:init(p)
    option.init(self,p)
    self.scale = 1
end
function menu_option:convert(value)
    return math.floor(value)
end
function menu_option:format(value)
    return tostring(self:convert(value))
end
function menu_option:set_value(value)
    menu.set(self.parent, self.entry, self:convert(value))
end
function menu_option:get_value()
    return menu.get(self.parent, self.name)
end
function menu_option:init_start() end

menu_time_option = class(menu_option)
function menu_time_option:format(value)
    return string.format("%2d:%02d", math.floor(value // 60), math.floor(value % 60))
end


ramp = {}

ramp.options = 
{
    apex_option {
        name = "Shutter",
        source = camera.shutter,
        property = "apex",
        min = -5,
        max = 12,
    },
    apex_option {
        name = "Aperture",
        source = camera.aperture,
        property = "apex",
        min = camera.aperture.min.apex,
        max = camera.aperture.max.apex
    },
    option {
        name = "ISO",
        source = camera.iso,
        property = "apex",
        min = 5,
        max = 11,
    },
    option {
        name = "EC",
        source = camera.ec,
        property = "value",
        min = -5,
        max = 5,
    },
    option {
        name = "Flash EC",
        source = camera.flash_ec,
        property = "value",
        min = -5,
        max = 5,
    },
    focus_option {
        name = "Focus",
        scale = 1,
    },
    wb_option {
        name = "WB",
        source = camera,
        property = "kelvin",
        scale = 100,
        offset = 5500,
    },
    interval_option {
        name = "Interval",
        source = interval,
        property = "time",
        scale = 1
    },
    menu_time_option {
        name = "Bulb Timer",
        parent = "Bulb Timer",
        entry = "Exposure duration",
        min = 1
    },
    menu_option {
        name = "AutoETTR",
        parent = "Shoot",
        entry = "Auto ETTR",
        min = 0,
        max = 1,
    },
    menu_option {
        name = "Slowest Shutter",
        parent = "Auto ETTR",
        entry = "Slowest shutter",
    },
    menu_option {
        name = "Midtone SNR",
        parent = "Auto ETTR",
        entry = "Midtone SNR limit",
    },
    menu_option {
        name = "Shadow SNR",
        parent = "Auto ETTR",
        entry = "Shadow SNR limit",
    },
    menu_option {
        name = "Dual ISO",
        parent = "Shoot",
        entry = "Dual ISO",
        min = 0,
        max = 1,
    },
    menu_option {
        name = "Recovery ISO",
        parent = "Dual ISO",
        entry = "Recovery ISO",
    },
    menu_option {
        name = "Bracketing",
        parent = "Shoot",
        entry = "Advanced Bracket",
        min = 0,
        max = 1,
    },
    menu_option {
        name = "Frames",
        parent = "Advanced Bracket",
        entry = "Frames",
        min = 0,
        max = 12,
    },
    menu_option {
        name = "EV",
        parent = "Advanced Bracket",
        entry = "EV increment",
    },
    menu_option {
        name = "Silent Pic",
        parent = "Shoot",
        entry = "Silent Picture",
        min = 0,
        max = 1,
    }
}

ramp.selected_option = 1
ramp.x = 1
ramp.y = 0
ramp.xmin = 1
ramp.ymin = -15
ramp.xmax = 10
ramp.ymax = 15
ramp.font = FONT.MED
ramp.edit_help = "INFO->Toggle Target | SET->Add Keyframe | Arrows->Move Cursor | MENU->Exit"

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
            if opt.keyframes then
                local prev_kf
                for j,kf in pairs(opt.keyframes) do
                    if kf.frame == count then
                        ramp.log:writef("Ramping %s (%d): %s\n", opt.name, count, opt:format(kf.value))
                        opt:set_value(kf.value)
                        break
                    elseif kf.frame > count and prev_kf then
                        local value = linear_ramp(prev_kf, kf, count)
                        ramp.log:writef("Ramping %s (%d): %s\n", opt.name, count, opt:format(value))
                        opt:set_value(value)
                        break
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
        self.x = self.x - 1
        if self.x < self.xmin then self.x = self.xmax end
    elseif k == KEY.WHEEL_LEFT then
        self.x = self.x - 10
        if self.x < self.xmin then self.x = self.xmax end
    elseif k == KEY.RIGHT then
        self.x = self.x + 1
        if self.x > self.xmax then self.xmax = self.x end
    elseif k == KEY.WHEEL_RIGHT then
        self.x = self.x + 10
        if self.x > self.xmax then self.xmax = self.x end
    elseif k == KEY.UP then
        self.y = self.y + 1
        if self.y > self.ymax then self.ymax = self.y end
    elseif k == KEY.WHEEL_UP then
        self.y = self.y + 10
        if self.y > self.ymax then self.ymax = self.y end
    elseif k == KEY.DOWN then
        self.y = self.y - 1
        if self.y < self.ymin then self.ymin = self.y end
    elseif k == KEY.WHEEL_DOWN then
        self.y = self.y - 10
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

function ramp:clear_all()
    for k,opt in pairs(self.options) do
        opt.keyframes = nil
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
        display.line(tx(self.xmin), ty(0), tx(self.xmax), ty(0), COLOR.GRAY)
        
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
        
        --help
        display.rect(0, display.height - FONT.SMALL.height - 2, display.width, FONT.SMALL.height + 2, COLOR.DARK_GRAY, COLOR.DARK_GRAY)
        display.print(self.edit_help, 1, display.height - FONT.SMALL.height - 1, FONT.SMALL, COLOR.LIGHT_GRAY, COLOR.DARK_GRAY)
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
            select = function(this)
                if this.value == 0 then
                    this.value = 1
                else
                    this.value = 0
                end
                ramp.menu.value = this.value
            end
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
        {
            name = "Clear Keyframes",
            select = function(this) ramp:clear_all() end
        },
    }
}

ramp.config = config.create
{
    enabled = false,
    ramp = {}
}
function ramp.config:saving() 
    self.data.enabled = ramp.menu.submenu["Enabled"].value
    self.data.ramp = {}
    for k,opt in pairs(ramp.options) do
        if opt.keyframes then
            self.data.ramp[opt.name] = opt.keyframes
        end
    end
end
--load config data
ramp.menu.value = ramp.config.data.enabled or 0
ramp.menu.submenu["Enabled"].value = ramp.menu.value
if ramp.config.data.ramp then
    for name,kf in pairs(ramp.config.data.ramp) do
        for k,opt in pairs(ramp.options) do
            if opt.name == name then
                opt.keyframes = kf
                break
            end
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
    local log = logger("RAMP.ERR")
    log:write(error)
    log:close()
    keys:anykey()
end