-- 5x_2.5k_10bit_iso100_24fps

  lv.start()
  console.hide()
  menu.close()

-- warnings 
if menu.get("FPS override", "Actual FPS", "") >= "49" and menu.get("FPS override", "Actual FPS", "") <= "61" then
  menu.set("Overlay", "Global Draw", "OFF")
  display.notify_box("Set cam to mv1080p and run script again")
  msleep(1000)
  display.notify_box("Set cam to mv1080p and run script again")
  msleep(1000)
  return
end

if camera.model_short ~= "EOSM" then
  if camera.mode ~= MODE.M then
    display.notify_box("set camera to 'M' and run script again")
    msleep(1000)
    display.notify_box("set camera to 'M' and run script again")
    msleep(1000)
    return
  end
end

while camera.mode ~= MODE.MOVIE do
  display.notify_box("enable MOVIE mode")
  msleep(1000)
end

-- enable sound
  menu.set("Sound recording", "Enable sound", "ON")
if menu.get("Sound recording", "Enable sound", "") ~= "ON" then
  menu.set("Overlay", "Global Draw", "OFF")
  display.notify_box("enable mlv_snd.mo and restart to record sound")
  msleep(1000)
  display.notify_box("enable mlv_snd.mo and restart to record sound")
  msleep(1000)
  return
end

-- workaround. May need an extra pass(if higher fps than actual)
while menu.get("FPS override", "Actual FPS", "") <= "23" or menu.get("FPS override", "Actual FPS", "") >= "25" or menu.get("Movie", "FPS override", "") == "OFF" do 
  lv.zoom = 1
  menu.set("Movie", "FPS override", "OFF")
  msleep(1000)

  if menu.get("FPS override", "Actual FPS", "") >= "23" and menu.get("FPS override", "Actual FPS", "") <= "24" then
    menu.set("FPS override", "Desired FPS", "24 (from 24)")
    camera.shutter.value = 1/50
  elseif menu.get("FPS override", "Actual FPS", "") >= "25" and menu.get("FPS override", "Actual FPS", "") <= "26" then
    menu.set("FPS override", "Desired FPS", "24 (from 25)")
    camera.shutter.value = 1/50
  elseif menu.get("FPS override", "Actual FPS", "") >= "29" and menu.get("FPS override", "Actual FPS", "") <= "30" then
    menu.set("FPS override", "Desired FPS", "24 (from 30)")
    camera.shutter.value = 1/50
  elseif menu.get("FPS override", "Actual FPS", "") >= "49" and menu.get("FPS override", "Actual FPS", "") <= "51" then
    menu.set("FPS override", "Desired FPS", "24 (from 50)")
    camera.shutter.value = 1/60
  elseif menu.get("FPS override", "Actual FPS", "") >= "59" and menu.get("FPS override", "Actual FPS", "") <= "60" then
    menu.set("FPS override", "Desired FPS", "24 (from 60)")
    camera.shutter.value = 1/60
  end

-- now turn FPS override to on
  menu.select("Movie", "FPS override")
  menu.open()     -- open ML menu
  key.press(KEY.SET)
  menu.close()
end

-- let´s go into zoom mode
  lv.zoom = 5

-- Overlay
  menu.set("Overlay", "Zebras", "OFF")
  menu.set("Overlay", "Magic Zoom", "OFF")
  menu.set("Overlay", "Cropmarks", "OFF")
  menu.set("Overlay", "Spotmeter", "OFF")
  menu.set("Overlay", "False color", "OFF")
  menu.set("Overlay", "Histogram", "OFF")
  menu.set("Overlay", "Waveform", "OFF")
  menu.set("Overlay", "Vectorscope", "OFF")
  menu.set("Display", "Clear overlays", "OFF")

-- Movie
  menu.set("Movie", "RAW video", "ON")
  menu.set("Movie", "HDR video", "OFF")
  menu.set("RAW video", "Resolution", 2560)
  menu.set("RAW video", "Data format", "10-bit lossless")
  menu.set("RAW video", "Preview", "Real-time")
  camera.iso.value=100
  menu.close()

-- done, turn on global draw
  menu.set("Overlay", "Global Draw", "LiveView")

-- go back to menu origin
  menu.select("Scripts")