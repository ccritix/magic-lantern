--adtg_gui helper

--[[ Script solely for starting a routine around adtg_gui
known issues atm. 100D needs global draw turned off before running the script, 
other than that, follow linked procedure and use with the right ML build version:
https://www.magiclantern.fm/forum/index.php?topic=9741.msg202679#msg202679
 ]]--


  console.hide()
  menu.close()
  menu.set("Overlay", "Global Draw", "OFF")
  msleep(1000)

-- warnings 
while camera.mode ~= MODE.MOVIE do
  display.notify_box("enable MOVIE mode")
  msleep(1000)
end

  menu.set("Overlay", "Focus Peak", "OFF")
  menu.set("Overlay", "Zebras", "OFF")
  menu.set("Overlay", "Cropmarks", "OFF")
  menu.set("Overlay", "Spotmeter", "OFF")
  menu.set("Overlay", "False color", "OFF")
  menu.set("Overlay", "Histogram", "OFF")
  menu.set("Overlay", "Waveform", "OFF")
  menu.set("Overlay", "Vectorscope", "OFF")
  menu.set("Display", "Clear overlays", "OFF")

  menu.open()     -- open ML menu
  menu.select("Debug", "ADTG Registers")
  menu.set("Debug", "ADTG Registers", "ON")
  msleep(1000)
  key.press(KEY.Q)
  key.press(KEY.SET)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.SET)
  msleep(1000)
  key.press(KEY.WHEEL_DOWN)
  key.press(KEY.SET)
  msleep(1000)
  key.press(KEY.SET)
  msleep(1000)
  key.press(KEY.Q)
  key.press(KEY.Q)

  menu.close()
  menu.set("Movie", "RAW video", "ON") 
  menu.set("RAW video", "Data format", "14-bit lossless")
  menu.set("Movie", "HDR video", "OFF")
  menu.set("RAW video", "Resolution", 3520)
  menu.set("Sound recording", "Enable sound", "OFF")
  menu.set("Movie", "FPS override", "OFF")
  menu.set("RAW video", "Preview", "Auto")
  msleep(1000)
 
  lv.zoom = 5
  menu.open()     -- open ML menu
  msleep(1000)
  menu.get("Debug", "ADTG Registers")
  key.press(KEY.Q)
  key.press(KEY.SET)
  key.press(KEY.WHEEL_UP)
  key.press(KEY.SET)
  msleep(2000)
  menu.close()
  key.press(KEY.MENU)
  msleep(1000)
  key.press(KEY.MENU)
  msleep(1000)
  lv.zoom = 5

if camera.model_short == "EOSM" then
  menu.set("Overlay", "Global Draw", "LiveView")
  menu.set("RAW video", "Preview", "Auto")
end

-- go back to menu origin
  menu.select("Debug", "ADTG Registers")
