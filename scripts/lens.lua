-- Manual Lens Information
-- Supplies lens information for manual lenses
-- Whenever there is a non-chipped lens detected, we prompt the user to select the attached manual lens from a list

require("ui")
require("config")
require("xmp")

lenses =
{
    { name = "My Lens", focal_length = 50 },
}

selected_index = 1
selector_instance = nil
xmp_instance = nil

function property.LENS_NAME:handler(value)
    if lens.is_chipped == false then
        task.create(select_lens)
    else
        if selector_instance ~= nil then
            selector_instance.cancel = true
        end
        if xmp_instance ~= nil then
            xmp_instance:disable()
        end
    end
end

function select_lens()
    menu.open()
    display.rect(0, 0, display.width, display.height, COLOR.BLACK, COLOR.BLACK)
    selector_instance = selector.create("Select Manual Lens", lenses, function(l) return l.name end, 600)
    if selector_instance:select() then
        selected_index = selector_instance.index
        for k,v in pairs(lenses[selected_index]) do
            lens[k] = v
        end
        if xmp_instance == nil then
            xmp_instance = xmp.create()
            xmp_instance:add_property(xmp.lens_name, function() return lens.name end)
            xmp_instance:add_property(xmp.focal_length, function() return lens.focal_length end)
            xmp_instance:add_property(xmp.aperture, function() return math.floor(camera.aperture.value * 10) end)
        end
        xmp_instance:enable()
    end
    selector_instance = nil
    menu.close()
end

if lens.is_chipped == false then
    task.create(select_lens)
end
