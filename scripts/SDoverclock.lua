-- sd overclock engine

-- Works with sd_uhs.mo enabled 
  console.hide()
if menu.get("Crop mode", "sd_uhs", "") ~= "enabled" and camera.model_short == "EOSM" then
if menu.get("Debug", "SD overclock", "MAY CAUSE DATA LOSS") == "MAY CAUSE DATA LOSS" then
  if menu.get("Movie", "RAW video", "") == "OFF" then
 display.notify_box("SD overclocking, please wait...")
    menu.set("Debug", "SD overclock", "ON")
    msleep(300)
 display.notify_box("patched")
 else
    menu.set("Movie", "RAW video", "OFF")
    msleep(50)
 display.notify_box("SD overclocking, please wait...")
    menu.set("Debug", "SD overclock", "ON")
    msleep(300)
    menu.set("Movie", "RAW video", "ON")
 display.notify_box("patched")
  end
 else
 display.notify_box("Please enable sd_uhs.mo")
    msleep(2000)
 display.notify_box("Please enable sd_uhs.mo")
    msleep(2000)
    return
end
end
