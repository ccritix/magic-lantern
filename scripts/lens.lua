-- Manual Lens Information
-- Supplies lens information for manual lenses
-- Whenever there is a non-chipped lens detected, we prompt the user to select the attached manual lens from a list

require("ui")

lenses =
{
    { name = "My Lens", focal_length = 50 },
}

selected_index = 1
selector_instance = nil

function property.LENS_NAME:handler(value)
    if lens.is_chipped == false then
        task.create(select_lens)
    elseif selector_instance ~= nil then
        selector_instance.cancel = true
    end
end

--TODO: write xmp sidecar metadata for CR2s

function select_lens()
    menu.open()
    selector_instance = selector.create("Select Manual Lens", lenses, function(l) return l.name end, 600)
    selector_instance:run()
    if not selector_instance.cancel then
        selected_index = selector_instance.index
        for k,v in ipairs(lenses[selected_index]) do
            lens[k] = v
        end
    end
    selector_instance = nil
    menu.close()
end

if lens.is_chipped == false then
    task.create(select_lens)
end
