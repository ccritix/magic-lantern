-- Manual Lens Information
-- Supplies lens information for manual lenses
-- Whenever there is a non-chipped lens detected, we prompt the user to select the attached manual lens from a list

require("ui")
require("config")
require("xmp")

lenses =
{
--  The following is for testing purposes. Comment out the following lines then either uncomment only the lenses
--  that you want to use from the list or add your own lenses. Tip: Put your most used lenses at the top of the list.

    { name = "My Lens", focal_length = 50 },
    { name = "My Other Lens", focal_length = 25 },

--  Zeiss ZF.2 manual lenses Nikon mount - these work with the lens profiles that ship with Adobe Camera Raw
--  { name = "Zeiss Distagon T* 2.8/15 ZF.2",        focal_length =  15, manual_aperture = 2.8 },
--  { name = "Zeiss Distagon T* 3.5/18 ZF.2",        focal_length =  18, manual_aperture = 3.5 },
--  { name = "Zeiss Distagon T* 2.8/21 ZF.2",        focal_length =  21, manual_aperture = 2.8 },
--  { name = "Zeiss Distagon T* 2.8/25 ZF.2",        focal_length =  25, manual_aperture = 2.8 },
--  { name = "Zeiss Distagon T* 2/25 ZF.2",          focal_length =  25, manual_aperture = 2   },
--  { name = "Zeiss Distagon T* 2/28 ZF.2",          focal_length =  28, manual_aperture = 2   },
--  { name = "Zeiss Distagon T* 2/35 ZF.2",          focal_length =  35, manual_aperture = 2   },
--  { name = "Zeiss Distagon T* 1.4/35 ZF.2",        focal_length =  35, manual_aperture = 1.4 },
--  { name = "Zeiss Makro-Planar T* 2/50 ZF.2",      focal_length =  50, manual_aperture = 2   },
--  { name = "Zeiss Planar T* 1.4/50 ZF.2",          focal_length =  50, manual_aperture = 1.4 },
--  { name = "Zeiss Planar T* 1.4/85 ZF.2",          focal_length =  85, manual_aperture = 1.4 },
--  { name = "Zeiss Makro-Planar T* 2/100 ZF.2",     focal_length = 100, manual_aperture = 2   },
--  { name = "Zeiss Apo Sonnar T* 2/135 ZF.2",       focal_length = 135, manual_aperture = 2   },

--  Samyang manual lenses - also branded as Rokinon and Bower. Cine versions use the same lens profile.
--  The lens profiles for Samyang manual lenses that ship with Adobe Camera raw must be modified in order
--  for automatic lens detection to work.
--  More information here: http://www.magiclantern.fm/forum/index.php?topic=18083.msg176261#msg176261
--  { name = "Samyang 8mm f/2.8 UMC Fisheye",        focal_length =   8, manual_aperture = 2.8 },
--  { name = "Samyang 8mm f/2.8 UMC Fisheye II",     focal_length =   8, manual_aperture = 2.8 }, --   8mm T3.1 Cine
--  { name = "Samyang 8mm f/3.5 UMC Fish-Eye CS",    focal_length =   8, manual_aperture = 3.5 },
--  { name = "Samyang 8mm f/3.5 UMC Fish-Eye CS II", focal_length =   8, manual_aperture = 3.5 }, --   8mm T3.8 Cine
--  { name = "Samyang 10mm f/2.8 ED AS NCS CS",      focal_length =  10, manual_aperture = 2.8 }, --  10mm T3.1 Cine
--  { name = "Samyang 12mm f/2 NCS CS",              focal_length =  12, manual_aperture = 2   }, --  12mm T2.2 Cine
--  { name = "Samyang 12mm f/2.8 ED AS NCS Fisheye", focal_length =  12, manual_aperture = 2.8 }, --  12mm T3.1 Cine
--  { name = "Samyang 14mm f/2.8 ED AS IF UMC",      focal_length =  14, manual_aperture = 2.8 }, --  14mm T3.1 Cine
--  { name = "Samyang 16mm f/2 ED AS UMC CS",        focal_length =  16, manual_aperture = 2   }, --  16mm T2.2 Cine
--  { name = "Samyang 21mm f/1.4 ED AS UMC CS",      focal_length =  21, manual_aperture = 1.4 }, --  21mm T1.5 Cine
--  { name = "Samyang 24mm f/1.4 ED AS IF UMC",      focal_length =  24, manual_aperture = 1.4 }, --  24mm T1.5 Cine
--  { name = "Samyang 35mm f/1.4 AS IF UMC",         focal_length =  35, manual_aperture = 1.4 }, --  35mm T1.5 Cine
--  { name = "Samyang 50mm f/1.2 AS UMC CS",         focal_length =  50, manual_aperture = 1.2 },
--  { name = "Samyang 50mm f/1.4 AS UMC",            focal_length =  50, manual_aperture = 1.4 }, --  50mm T1.5 Cine
--  { name = "Samyang 85mm f/1.4 AS IF UMC",         focal_length =  85, manual_aperture = 1.4 }, --  85mm T1.5 Cine
--  { name = "Samyang 100mm f/2.8 ED UMC MACRO",     focal_length = 100, manual_aperture = 2.8 }, -- 100mm T3.1 Cine
--  { name = "Samyang 135mm f/2 ED UMC",             focal_length = 135, manual_aperture = 2   }, -- 135mm T2.2 Cine
--  { name = "Samyang 300mm f/6.3 ED UMC CS",        focal_length = 300, manual_aperture = 6.3 },
}

selector_instance = selector.create("Select Manual Lens", lenses, function(l) return l.name end, 600)

lens_config = config.create({})

if lens_config.data ~= nil and lens_config.data.name ~= nil then
    for i,v in ipairs(lenses) do
        if v.name == lens_config.data.name then
            selector_instance.index = i
            break
        end
    end
end

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
    lens_config.data = { name = lenses[selector_instance.index].name }
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
