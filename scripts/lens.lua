-- Manual Lens Information
-- Supplies lens information for manual lenses
-- Whenever there is a non-chipped lens detected, we prompt the user to select the attached manual lens from a list

require("ui")
require("config")
require("xmp")

lenses =
{
    { name = "My Lens", focal_length = 50 },
    { name = "My Other Lens", focal_length = 25 },
}

selector_instance = selector.create("Select Manual Lens", lenses, function(l) return l.name end, 600)

xmp:add_property(xmp.lens_name, function() return lens.name end)
xmp:add_property(xmp.focal_length, function() return lens.focal_length end)
xmp:add_property(xmp.aperture, function() return math.floor(camera.aperture.value * 10) end)

function property.LENS_NAME:handler(value)
    if lens.is_chipped == false then
        task.create(select_lens)
    else
        if selector_instance ~= nil then
            selector_instance.cancel = true
        end
        xmp:stop()
    end
end

function select_lens()
    if #lenses > 1 then
        local menu_already_open = menu.visible
        if not menu_already_open then
            menu.open()
            display.rect(0, 0, display.width, display.height, COLOR.BLACK, COLOR.BLACK)
        end
        if selector_instance:select() then
            update_lens()
        end
        if not menu_already_open then
            menu.close()
        end
    elseif #lenses == 1 then
        update_lens()
    end
end

function update_lens()
    for k,v in pairs(lenses[selector_instance.index]) do
        lens[k] = v
    end
    xmp:start()
end

lens_menu = menu.new
{
    parent = "Lens Info Prefs",
    name = "Manual Lens",
    help = "Info to use for attached non-chipped lens",
    icon_type = ICON_TYPE.ACTION,
    select = function() 
        if lens.is_chipped == false then
            task.create(select_lens)
        end
    end,
    rinfo = function()
        return lens.name
    end,
    warning = function()
        if lens.is_chipped then
            return "Chipped lens is attached"
        end
    end
}

if lens.is_chipped == false then
    task.create(select_lens)
end
