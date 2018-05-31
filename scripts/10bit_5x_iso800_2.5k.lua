-- 10bit_5x_iso800_2.5k

menu.set("Overlay", "Global Draw", "OFF")
console.hide()
menu.close()
while camera.mode ~= MODE.MOVIE do
   display.notify_box("enable MOVIE mode")
   msleep(1000)
end

   display.notify_box("10bit_5x_iso800_2.5k")
   msleep(1000)

    menu.set("Sound recording", "Enable sound", "ON")
while menu.get("Sound recording", "Enable sound", "") ~= "ON" do
   display.notify_box("enable mlv_snd.mo and restart to record sound")
   msleep(1000)
end

   display.notify_box("FPS override ON?")
   msleep(1000)

    menu.set("Movie", "RAW video", "ON")  
    menu.set("RAW video", "Resolution", 2560)
    menu.set("RAW video", "Data format", "10-bit lossless")
    menu.set("RAW video", "Preview", "Real-time") 
    camera.shutter.value = 1/50
    camera.iso.value=800
    lv.zoom = 5
    menu.set("Display", "Clear overlays", "OFF")
    menu.set("Overlay", "Zebras", "OFF")
    menu.set("Overlay", "Magic Zoom", "OFF")
    menu.set("Overlay", "Cropmarks", "OFF")
    menu.set("Overlay", "Spotmeter", "OFF")
    menu.set("Overlay", "False color", "OFF")
    menu.set("Overlay", "Histogram", "OFF")
    menu.set("Overlay", "Waveform", "OFF")
    menu.set("Overlay", "Vectorscope", "OFF")
    menu.set("Display", "Clear overlays", "OFF")
    menu.set("Overlay", "Global Draw", "LiveView")
    menu.close()