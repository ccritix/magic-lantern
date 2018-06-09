-- darkframe creator

--[[
Will create 3 second MLV files with following settings:
* iso 100-6400 iso 
* FPS override set to 24 
* mv1080p (1920x1080) 12-bit lossless, 14bit-lossless, 12-bit, 14bit
* 5xZoom (highest setting) 12-bit lossless(For 5D3 also 14bit lossless and 12-bit)
* Movie crop mode (whatever set to when starting) 12-bit lossless(For 5D3 also 14bit lossless and 12-bit)
* For further darkframe automation on mac check "Switch" https://www.magiclantern.fm/forum/index.php?topic=15108.0
* On windows check "batch_mlv" https://www.magiclantern.fm/forum/index.php?topic=10526.msg188558#msg188558
* Note that processing will take a while so why not have a coffe while waiting...
* don´t forget to film with your lens cap on
--]]

  lv.start()
  lv.zoom = 1
  console.hide()
  menu.close()

-- warnings 
while camera.mode ~= MODE.MOVIE do
  display.notify_box("enable MOVIE mode")
  msleep(1000)
end

  menu.set("Movie", "FPS override", "OFF")
  msleep(1000)
if menu.get("FPS override", "Actual FPS", "") >= "49" and menu.get("FPS override", "Actual FPS", "") <= "61" then
  menu.set("Overlay", "Global Draw", "OFF")
  display.notify_box("Set cam to mv1080p and run script again")
  msleep(1000)
  display.notify_box("Set cam to mv1080p and run script again")
  msleep(1000)
  return
end

-- some pointers before starting
  display.notify_box("Put your lens cap on when filming")
  msleep(2000)
  display.notify_box("Put your lens cap on when filming")
  msleep(2000)
  display.notify_box("This will take about five minutes to finish")
  msleep(2000)
  display.notify_box("This will take about five minutes to finish")
  msleep(2000)

-- Overlay
  menu.set("Overlay", "Global Draw", "OFF")

-- global start off setting
if menu.get("Movie", "Movie crop mode", "") == "ON" then
  menu.select("Movie", "Movie crop mode")
  menu.open()     -- open ML menu
  key.press(KEY.SET)
  menu.close()
end

-- if we´re in crop mode
  if menu.get("Movie", "Crop mode", "") == "3x3 720p" then
    menu.select("Movie", "Crop mode")
    menu.open()     -- open ML menu
    key.press(KEY.SET)
    menu.close()
  end

-- Movie settings
  camera.iso.value=100
  camera.shutter.value = 1/50
  menu.set("RAW video", "Data format", "12-bit lossless")
  menu.set("Sound recording", "Enable sound", "OFF")
  menu.set("Movie", "HDR video", "OFF")
  menu.set("Movie", "RAW video", "ON") 
  menu.set("RAW video", "Resolution", 1920)
  menu.set("RAW video", "Preview", "Auto")

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

-- ISO
  i = 100
--------------------------------------  
-- mv1080p (1920x1080) 24 FPS override
-------------------------------------- 
  display.notify_box("12-bit lossless mv1080p")
  msleep(2000)
while menu.get("RAW video", "Data format", "") ~= "14-bit" do 
  camera.iso.value=(i)
  key.press(KEY.REC)
  msleep(4000)
  movie.stop()
  msleep(3000)
  if menu.get("ISO", "Equivalent ISO", "") ~= "6400" then
    i = i * 2
  else
    i = 100
    if menu.get("RAW video", "Data format", "") == "12-bit lossless" then
      display.notify_box("14-bit lossless mv1080p")
      menu.set("RAW video", "Data format", "14-bit lossless")
    elseif
      menu.get("RAW video", "Data format", "") == "14-bit lossless" then
      display.notify_box("12-bit mv1080p")
      menu.set("RAW video", "Data format", "12-bit")
    elseif
      menu.get("RAW video", "Data format", "") == "12-bit" then
      display.notify_box("14-bit mv1080p")
      menu.set("RAW video", "Data format", "14-bit")
    end
  end
end

-- last darkframe round 14-bit
if camera.model_short == "5D3" then
  i = 100
  camera.iso.value=100
  while menu.get("ISO", "Equivalent ISO", "") ~= "6400" do 
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
    i = i * 2
    camera.iso.value=(i)
    msleep(1000)
  end
-- last 6400 iso
  if menu.get("ISO", "Equivalent ISO", "") == "6400" then
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
  end
end
-----------------------------------------  
-- 5xZoom highest setting 24 FPS override 
----------------------------------------- 
  i = 100
  lv.zoom = 5
  camera.iso.value=100
  camera.shutter.value = 1/50
  menu.set("RAW video", "Data format", "12-bit lossless")
  menu.set("RAW video", "Resolution", 2880)
  display.notify_box("12-bit lossless 5xzoom")

  msleep(2000)
while menu.get("RAW video", "Data format", "") ~= "12-bit" do 
  camera.iso.value=(i)
  key.press(KEY.REC)
  msleep(4000)
  movie.stop()
  msleep(3000)
  if menu.get("ISO", "Equivalent ISO", "") ~= "6400" then
    i = i * 2
  else
    i = 100
    if menu.get("RAW video", "Data format", "") == "12-bit lossless" and camera.model_short == "5D3" then
      display.notify_box("14-bit lossless 5xzoom")
      menu.set("RAW video", "Data format", "14-bit lossless")
    elseif
      menu.get("RAW video", "Data format", "") == "14-bit lossless" and camera.model_short == "5D3" then
      display.notify_box("12-bit 5xzoom")
      menu.set("RAW video", "Data format", "12-bit")
    elseif camera.model_short ~= "5D3" then
      menu.set("RAW video", "Data format", "12-bit")
    end
  end
end

-- last darkframe round 12-bit
if camera.model_short == "5D3" then
  i = 100
  camera.iso.value=100
  while menu.get("ISO", "Equivalent ISO", "") ~= "6400" do
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
    i = i * 2
    camera.iso.value=(i)
    msleep(1000)
  end

-- last 6400 iso
  if menu.get("ISO", "Equivalent ISO", "") == "6400" then
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
  end
end
----------------------------------
-- Movie crop mode 24 FPS override
----------------------------------
  lv.zoom = 1
  i = 100
  camera.iso.value=100
  camera.shutter.value = 1/50
  menu.set("RAW video", "Data format", "12-bit lossless")
-- 100D is buggy here
if camera.model_short ~= "100D" then
 if menu.get("Movie", "Movie crop mode", "") == "OFF" then
   menu.select("Movie", "Movie crop mode")
   menu.open()     -- open ML menu
   key.press(KEY.SET)
   menu.close()
   display.notify_box("12-bit lossless Movie crop mode")

  msleep(2000)
while menu.get("RAW video", "Data format", "") ~= "12-bit" do 
  camera.iso.value=(i)
  key.press(KEY.REC)
  msleep(4000)
  movie.stop()
  msleep(3000)
  if menu.get("ISO", "Equivalent ISO", "") ~= "6400" then
    i = i * 2
  else
    if menu.get("RAW video", "Data format", "") == "12-bit lossless" and camera.model_short == "5D3" then
      display.notify_box("14-bit lossless Movie crop mode")
      menu.set("RAW video", "Data format", "14-bit lossless")
    elseif
      menu.get("RAW video", "Data format", "") == "14-bit lossless" and camera.model_short == "5D3" then
      display.notify_box("12-bit Movie crop mode")
      menu.set("RAW video", "Data format", "12-bit")
    elseif camera.model_short ~= "5D3" then
      menu.set("RAW video", "Data format", "12-bit")
    end
  end
end

-- last darkframe round 12-bit
if camera.model_short == "5D3" then
  i = 100
  camera.iso.value=100
  while menu.get("ISO", "Equivalent ISO", "") ~= "6400" do
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
    i = i * 2
    camera.iso.value=(i)
    msleep(1000)
  end

-- last 6400 iso
  if menu.get("ISO", "Equivalent ISO", "") == "6400" then
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
  end
end

  if menu.get("Movie", "Movie crop mode", "") == "ON" then
    menu.select("Movie", "Movie crop mode")
    menu.open()     -- open ML menu
    key.press(KEY.SET)
    menu.close()
  end
 end
end

-- Starting point
  camera.iso.value=100
  camera.shutter.value = 1/50
  menu.set("RAW video", "Data format", "12-bit lossless")
----------------------------------
-- Crop mode 24 FPS override EOSM
----------------------------------
  display.notify_box("12-bit lossless Crop mode")
if camera.model_short == "EOSM" then
  lv.zoom = 1
  i = 100
  camera.iso.value=100
  camera.shutter.value = 1/60
  menu.set("RAW video", "Data format", "12-bit lossless")
  if menu.get("Movie", "Crop mode", "") == "OFF" then
    menu.select("Movie", "Crop mode")
    menu.open()     -- open ML menu
    key.press(KEY.SET)
    menu.close()
    display.notify_box("12-bit lossless Crop mode")

    msleep(2000)
while menu.get("RAW video", "Data format", "") ~= "14-bit" do 
  camera.iso.value=(i)
  key.press(KEY.REC)
  msleep(4000)
  movie.stop()
  msleep(3000)
  if menu.get("ISO", "Equivalent ISO", "") ~= "6400" then
    i = i * 2
  else
    i = 100
    if menu.get("RAW video", "Data format", "") == "12-bit lossless" then
      display.notify_box("14-bit Croprec")
      menu.set("RAW video", "Data format", "14-bit lossless")
    elseif
      menu.get("RAW video", "Data format", "") == "14-bit lossless" then
      display.notify_box("12-bit Croprec")
      menu.set("RAW video", "Data format", "12-bit")
    elseif
      menu.get("RAW video", "Data format", "") == "12-bit" then
      display.notify_box("14-bit Croprec")
      menu.set("RAW video", "Data format", "14-bit")
    end
  end
end

-- last darkframe round 14-bit
if camera.model_short == "5D3" then
  i = 100
  camera.iso.value=100
  while menu.get("ISO", "Equivalent ISO", "") ~= "6400" do 
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
    i = i * 2
    camera.iso.value=(i)
    msleep(1000)
  end
-- last 6400 iso
  if menu.get("ISO", "Equivalent ISO", "") == "6400" then
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
  end
end

-- disable Crop mode
   menu.open()     -- open ML menu
   key.press(KEY.SET)
   menu.close()
end

-- Starting point
  camera.iso.value=100
  camera.shutter.value = 1/50
  menu.set("RAW video", "Data format", "12-bit lossless")
end
----------------------------------
-- Crop mode 24 FPS override other cameras
----------------------------------
if menu.get("Movie", "Crop mode", "") ~= "OFF" then
  display.notify_box("Crop_rec module not enabled, all done!")
  msleep(1000)
  display.notify_box("Crop_rec module not enabled, all done!")
  msleep(2000)
  return
end

while menu.get("Movie", "Crop mode", "") == "OFF" do
  display.notify_box("set cam to mv720p then turn on Crop mode")
  msleep(1000)
end

  menu.close()
  msleep(2000)
while menu.get("RAW video", "Data format", "") ~= "14-bit" do 
  camera.iso.value=(i)
  key.press(KEY.REC)
  msleep(4000)
  movie.stop()
  msleep(3000)
  if menu.get("ISO", "Equivalent ISO", "") ~= "6400" then
    i = i * 2
  else
    i = 100
    if menu.get("RAW video", "Data format", "") == "12-bit lossless" then
      display.notify_box("14-bit Croprec")
      menu.set("RAW video", "Data format", "14-bit lossless")
    elseif
      menu.get("RAW video", "Data format", "") == "14-bit lossless" then
      display.notify_box("12-bit Croprec")
      menu.set("RAW video", "Data format", "12-bit")
    elseif
      menu.get("RAW video", "Data format", "") == "12-bit" then
      display.notify_box("14-bit Croprec")
      menu.set("RAW video", "Data format", "14-bit")
    end
  end
end

-- last darkframe round 14-bit
if camera.model_short == "5D3" then
  i = 100
  camera.iso.value=100
  while menu.get("ISO", "Equivalent ISO", "") ~= "6400" do 
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
    i = i * 2
    camera.iso.value=(i)
    msleep(1000)
  end
-- last 6400 iso
  if menu.get("ISO", "Equivalent ISO", "") == "6400" then
    key.press(KEY.REC)
    msleep(4000)
    movie.stop()
    msleep(3000)
  end
end

-- disable Crop mode
   menu.open()     -- open ML menu
   key.press(KEY.SET)
   menu.close()

-- Starting point
  camera.iso.value=100
  camera.shutter.value = 1/50
  menu.set("RAW video", "Data format", "12-bit lossless")

















