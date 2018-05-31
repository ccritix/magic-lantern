-- 10bit_5x_iso100_2.5k_24fps

menu.set("Overlay", "Global Draw", "OFF")
console.hide()
menu.close()
while camera.mode ~= MODE.MOVIE do
   display.notify_box("enable MOVIE mode")
   msleep(1000)
end

   display.notify_box("10bit_5x_iso100_2.5k_24fps")
   msleep(1000)

    menu.set("Sound recording", "Enable sound", "ON")
while menu.get("Sound recording", "Enable sound", "") ~= "ON" do
   display.notify_box("enable mlv_snd.mo and restart to record sound")
   msleep(1000)
end

if menu.get("FPS override", "Desired FPS", "") ~= "24 (from 30)" then
   menu.close()
   display.notify_box("Set Desired FPS to 24")
   msleep(2000)
   display.notify_box("then rerun this script")
   msleep(2000)
   return
   else
   display.notify_box("FPS override ON?")
   msleep(1000)
end

    menu.set("Movie", "RAW video", "ON")   
    camera.shutter.value = 1/50
    camera.iso.value=100
    lv.zoom = 5
    menu.set("RAW video", "Resolution", 2560)
    menu.set("Display", "Clear overlays", "OFF")
    menu.set("RAW video", "Data format", "10-bit lossless")
    menu.close()
