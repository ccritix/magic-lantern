-- enable SD overclocking

  console.hide()
  msleep(200) -- let´s start off with a nap
if menu.get("Debug", "SD overclock", "MAY CAUSE DATA LOSS") == "MAY CAUSE DATA LOSS" then
  if menu.get("Movie", "RAW video", "") == "OFF" then
      if camera.model_short == "EOSM" 
	and menu.get("Movie", "Crop mode", "") == "1x3 1736x2150" 
	 and menu.get("Display", "Clear overlays", "") ~= "HalfShutter" 
	  and menu.get("RAW video", "Preview", "Framing") then
           display.notify_box("enable Clear overlays Halfshutter")
          msleep(2000) 
        end
 display.notify_box("SD overclocking, please wait...")
    menu.set("Debug", "SD overclock", "ON")
    msleep(3000)
 display.notify_box("patched")
    msleep(1000)
 else
    menu.set("Movie", "RAW video", "OFF")
      if camera.model_short == "EOSM" 
	and menu.get("Movie", "Crop mode", "") == "1x3 1736x2150" 
	 and menu.get("Display", "Clear overlays", "") ~= "HalfShutter" 
	  and menu.get("RAW video", "Preview", "Framing") then
           display.notify_box("enable Clear overlays Halfshutter")
          msleep(2000) 
        end
 display.notify_box("SD overclocking, please wait...")
    menu.set("Debug", "SD overclock", "ON")
    msleep(3000)
    menu.select("Movie", "RAW video")
    msleep(100)
    menu.open()     -- open ML menu
    key.press(KEY.SET)
    msleep(100)
    menu.close()
 display.notify_box("patched")
  end
 else
 display.notify_box("Please enable sd_uhs.mo")
    msleep(2000)
 display.notify_box("Please enable sd_uhs.mo")
    msleep(2000)
    return
end
