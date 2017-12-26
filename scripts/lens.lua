-- Manual Lens Information
-- Supplies lens information for manual lenses
-- Whenever there is a non-chipped lens detected or a manual lens with AF Confirm Chip, we prompt the user to select the attached manual lens from a list

require("ui")
require("config")
require("xmp")

lenses =
{
--  The following is for testing purposes. Comment out the following lines then either uncomment only the lenses
--  that you want to use from the list or add your own lenses. Tip: Put your most used lenses at the top of the list.

    { name = "My Lens", focal_length = 50 },
    { name = "My Other Lens", focal_length = 25, manual_aperture = 2.8 },
    { name = "My Zoom Lens", focal_length = 25, manual_aperture = 4, focal_length = 105, focal_min = 70, focal_max = 200 },

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

--  Nikon lenses
--  { name = "Nikon Zoom Ais ED 50-300",             focal_length = 300, manual_aperture = 4.5 },
--  { name = "Nikon AF NIKKOR 28mm f/1.4D",          focal_length =  28, manual_aperture = 1.4 },


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


--  Lensbaby lenses

--  { name = "Lensbaby Sweet 35",                    focal_length =  35, manual_aperture = 2.5 },
--  { name = "Lensbaby Sweet 50",                    focal_length =  50, manual_aperture = 2.5 },
--  { name = "Lensbaby Twist 60",                    focal_length =  60, manual_aperture = 2.5 },
--  { name = "Lensbaby Edge 50",                     focal_length =  50, manual_aperture = 3.2 },
--  { name = "Lensbaby Edge 80",                     focal_length =  80, manual_aperture = 2.8 },
--  { name = "Lensbaby Circular Fisheye",            focal_length = 5.8, manual_aperture = 3.5 },
--  { name = "Lensbaby Soft Focus Optic",            focal_length =  50, manual_aperture = 2   },
--  { name = "Lensbaby Double Glass Optic",          focal_length =  50, manual_aperture = 2   },
--  { name = "Lensbaby Single Glass Optic",          focal_length =  50, manual_aperture = 2   },
--  { name = "Lensbaby Plastic Optic",               focal_length =  50, manual_aperture = 2   },
--  { name = "Lensbaby Pinhole Optic",               focal_length =  50, manual_aperture = 19  },
--  { name = "Lensbaby Fisheye Optic",               focal_length =  12, manual_aperture = 4   },
--  { name = "Lensbaby Velvet 56",                   focal_length =  56, manual_aperture = 1.6 },
--  { name = "Lensbaby Velvet 85",                   focal_length =  85, manual_aperture = 1.8 },
--  { name = "Lensbaby Creative Aperture",           focal_length =  50, manual_aperture = 2   },

}

-- f-number values in 1/2 Stop
Fnumbers = {"1.0","1.2","1.4","1.7","2","2.4","2.8","3.3","4","4.8","5.6","6.7","8","9.5","11","13","16","19","22","27","32"}

selector_instance = selector.create("Select Manual Lens", lenses, function(l) return l.name end, 600)

lens_config = config.create({})

lensSelected = false

if lens_config.data ~= nil and lens_config.data.name ~= nil then
    for i,v in ipairs(lenses) do
        if v.name == lens_config.data.name then
            selector_instance.index = i
            break
        end
    end
end

-- Property to be written in .xmp file
xmp:add_property(xmp.lens_name, function() return lens.name end)
xmp:add_property(xmp.focal_length, function() return lens.focal_length end)
xmp:add_property(xmp.aperture, function() return math.floor(camera.aperture.value * 10) end)
xmp:add_property(xmp.lens_serial, function() return lens.serial end)

-- Helper function
function is_manual_lens()
  if (lens.id == 0 or lens.id == "(no lens)" or
      lens.name == "1-65535mm" or lens.focal_length == "1-65535mm") then
    return true
  else
    return false
  end
end

-- Function used to make sure all additional attributes are correct when switching lens,
-- as if the new lens doesn't have the same attribute, old values don't get overwritten in lens_info
-- Get called in update_lens() before setting value of the new lens
function reset_lens_values()
  lens.focal_length = 0
  lens.focal_min = 0
  lens.focal_max = 0
  lens.serial = 0
end

-- Get called in update_lens() after selecting a lens
-- Get called when changing aperture or focal by menu
function update_xmp()
  lens_config.data = { name = lenses[selector_instance.index].name }
  lens_config.data = { name = lenses[selector_instance.index].focal_length }
  lens_config.data = { name = lenses[selector_instance.index].manual_aperture }
  lens_config.data = { name = lenses[selector_instance.index].serial }
end

--  Handler for lens_name property
--  Get Called when:
--  Switching lens
--  Switching shoot mode
function property.LENS_NAME:handler(value)
    if is_manual_lens() then
        task.create(select_lens)
    else
      -- Not a manual Lens, no need to write sidecar file
      if selector_instance ~= nil then
          selector_instance.cancel = true
      end
      xmp:stop()
      -- Clear flag for next run
      lensSelected = false
    end
end

--  Handler for LV Lens Length property
--  Get Called when entering LV
--  Otherwise a 50mm focal length will be displayed
function property.LV_LENS:handler(value)
    -- Update length only if we are using a manual lens
    if lensSelected == true then
      -- Update attribute from selected lens
      lens.focal_length = lens_menu.submenu["Focal Length"].value
    end
end

-- Open lens selection Menu
function select_lens()
    if #lenses > 1 then
        local menu_already_open = menu.visible
        if not menu_already_open then
            menu.open()
            display.rect(0, 0, display.width, display.height, COLOR.BLACK, COLOR.BLACK)
        end
        if selector_instance:select() then
            update_lens()
            update_menu()
        end
        if not menu_already_open then
            menu.close()
        end
    elseif #lenses == 1 then
        update_lens()
        update_menu()
    end
end

-- Copy lens attribute from lenses and write to .xmp file
function update_lens()
    -- Reset lens_info structure to get correct values in Lens Info Menu and Metadata
    reset_lens_values()
    -- Update attribute from selected lens
    for k,v in pairs(lenses[selector_instance.index]) do
        lens[k] = v
    end
    -- Update content of lens.cfg
    update_xmp()
    -- Allow to write sidecar
    xmp:start()
    -- Update flag
    lensSelected = true
    -- Allow to write values for Lens Info and MLV file metadata
    lens.exists = true
end

lens_menu = menu.new
{
    parent = "Lens Info Prefs",
    name = "Manual Lens",
    help = "Info to use for attached non-chipped lens",
    icon_type = ICON_TYPE.ACTION,
    submenu =
    {
        {
          name = "Lens",
          help    = "Select Manual Lens",
          select = function()
                    if is_manual_lens() or lensSelected == true then
                      task.create(select_lens)
                   end end,
          rinfo = function()
                    return lens.name
                  end,
          warning = function()
                      if lensSelected == false then
                        return "this value is not supported for non-manual lens"
                    end end
        },
        {
            name    = "Focal Length",
            help    = "Set Focal Length to metadata",
            -- Min and Max are updated when a lens is selected from menu
            min     = 8,
            max     = 400,
            unit    = UNIT.DEC,
            -- Update Focal Length with selected value from submenu
            update = function(this)
                      -- A "0" is returned by menu when focal_min and focal_max are missing from lens attribute
                      if lensSelected == true and this.value ~= 0 then
                        lens.focal_length = this.value
                        update_xmp()
                      else
                        -- Reset menu value to the corrected one
                        this.value = lens.focal_length
                      end end,
            warning = function()
                        if lensSelected == false then
                          return "this value is not supported for non-manual lens"
                        else if lens.focal_min == lens.focal_max then
                          return "Chan be changed only for manual-focus Zoom lens"
                        end
                      end end,
        },
        {
            name    = "Aperture",
            help    = "Set Aperture to metadata",
            choices = Fnumbers,
            -- Update Aperture with selected value from submenu
            update = function(this)
                      if lensSelected == true then
                        lens.manual_aperture = tonumber(this.value)
                        update_xmp()
                      else
                        -- Reset menu value to the corrected one
                        this.value = lens.manual_aperture
                      end end,
            warning = function()
                        if lensSelected == false then
                          return "this value is not supported for non-manual lens"
                      end end,
        }
    },
    rinfo = function()
        return lens.name
    end,
    warning = function()
        if is_manual_lens() == false then
            return "Chipped lens is attached"
        end
    end
}

-- Update the menu with values for Focal Length and Aperture from selected Lens
-- To be called when switching manual lens
function update_menu()
  lens_menu.submenu["Focal Length"].value = lens.focal_length
  lens_menu.submenu["Focal Length"].min = lens.focal_min
  lens_menu.submenu["Focal Length"].max = lens.focal_max
  lens_menu.submenu["Aperture"].value = lens.manual_aperture
end


-- Check lens on start
if is_manual_lens() then
    task.create(select_lens)
end
