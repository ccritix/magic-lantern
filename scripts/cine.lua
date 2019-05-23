-- cinema 2:35.1  

-- Sets all parameters for anamorphic 2:35.1

  lv.start()
  lv.zoom = 1
  console.hide()
  menu.close()

-- end this script if not eosm/700d/100D/650D
if camera.model_short ~= "EOSM" and camera.model_short ~= "700D" and camera.model_short ~= "100D" and camera.model_short ~= "650D" and camera.model_short ~= "5D3" then
   display.notify_box("Script not working on this cam")
   menu.set("cinema 2:35.1", "Autorun", "OFF")
  msleep(2000)
   return
end

-- this script should never run on Autorun, menu.set not working so running menu.select instead
if menu.get("cinema 2:35.1", "Autorun", "") == "ON" then
   menu.select("Scripts", "cinema 2:35.1")
   menu.open()     -- open ML menu
   key.press(KEY.SET)
   key.press(KEY.WHEEL_DOWN)
   key.press(KEY.WHEEL_DOWN)
   key.press(KEY.SET)
   menu.close()
   display.notify_box("cinema Autorun is now disabled!")
  msleep(3000)
   return
end

-- warnings 
while camera.mode ~= MODE.MOVIE do
  display.notify_box("enable MOVIE mode")
  msleep(1000)
end

-- enable sound
  menu.set("Sound recording", "Enable sound", "ON")
if menu.get("Sound recording", "Enable sound", "") ~= "ON" then
  display.notify_box("enable mlv_snd.mo and mlv_lite.mo")
  msleep(2000)
  display.notify_box("enable mlv_snd.mo and mlv_lite.mo")
  msleep(2000)
  return
end

-- crop mode
    menu.set("Movie", "Crop mode", "5K anamorphic")
if camera.model_short == "100D" then
    menu.set("Movie", "Crop mode", "anamorphic rewired")
end
if camera.model_short == "5D3" then
    menu.set("Movie", "Crop mode", "anamorphic")
end
    menu.set("Crop mode", "bitdepth", "10 bit")
    menu.set("Crop mode", "ratios", "2.35:1")
  msleep(300)

-- enable crop_rec.mo. Checking first after trying to enable 5k preset
if menu.get("Movie", "Crop mode", "") ~= "5K anamorphic" and menu.get("Movie", "Crop mode", "") ~= "anamorphic rewired" and menu.get("Movie", "Crop mode", "") ~= "anamorphic" then
  display.notify_box("enable crop_rec.mo")
  msleep(1000)
  display.notify_box("enable crop_rec.mo")
  msleep(1000)
  return
end

-- movie
  menu.set("Movie", "FPS override", "OFF")
  menu.set("Movie", "HDR video", "OFF")
  menu.set("RAW video", "Crop rec preview", "auto mode")
  menu.set("RAW video", "Data format", "14-bit lossless") 
  menu.set("RAW video", "Preview", "Framing")
  menu.select("Movie", "RAW video")
  msleep(100)
  menu.open()
  msleep(100)
  key.press(KEY.Q)
  msleep(100)
  key.press(KEY.SET)
  msleep(100)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.WHEEL_DOWN)
  menu.set("RAW video", "Aspect ratio", "1:2")

-- Overlay
  menu.set("Overlay", "Focus Peak", "OFF")
  menu.set("Overlay", "Zebras", "OFF")
  menu.set("Overlay", "Magic Zoom", "OFF")
  menu.set("Overlay", "Cropmarks", "OFF")
  menu.set("Overlay", "Spotmeter", "OFF")
  menu.set("Overlay", "False color", "OFF")
  menu.set("Overlay", "Histogram", "OFF")
  menu.set("Overlay", "Waveform", "OFF")
  menu.set("Overlay", "Vectorscope", "OFF")
  menu.set("Display", "Clear overlays", "OFF")

-- done, turn on global draw
  menu.set("Overlay", "Global Draw", "LiveView")

-- go back to menu origin
  menu.select("Movie", "Crop mode")
  msleep(200)
  menu.open()
  msleep(200)
  menu.close()
  msleep(600)
  key.press(KEY.MENU)
  msleep(600)
  key.press(KEY.MENU)
  msleep(600)

-- success!
if camera.model_short == "100D" then
   display.notify_box("anamorphic rewired is all set")
elseif camera.model_short == "5D3" then
   display.notify_box("5.5K anamorphic is all set")
else
   display.notify_box("4.5K anamorphic is all set")
end

