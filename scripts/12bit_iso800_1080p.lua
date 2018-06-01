-- 12bit_iso800_1080p

menu.set("Overlay", "Global Draw", "OFF")
console.hide()
menu.close()
while camera.mode ~= MODE.MOVIE do
   display.notify_box("enable MOVIE mode")
   msleep(1000)
end

    menu.set("Sound recording", "Enable sound", "ON")
while menu.get("Sound recording", "Enable sound", "") ~= "ON" do
   display.notify_box("enable mlv_snd.mo and restart to record sound")
   msleep(1000)
end

    lv.zoom = 1
    menu.set("Overlay", "Focus Peak", "OFF")
    menu.set("Overlay", "Zebras", "OFF")
    menu.set("Overlay", "Magic Zoom", "F+HS, Med, TL, 2:1")
    menu.set("Overlay", "Cropmarks", "OFF")
    menu.set("Overlay", "Spotmeter", "OFF")
    menu.set("Overlay", "False color", "OFF")
    menu.set("Overlay", "Histogram", "OFF")
    menu.set("Overlay", "Waveform", "OFF")
    menu.set("Overlay", "Vectorscope", "OFF")
    menu.set("Display", "Clear overlays", "OFF")
    menu.set("Overlay", "Global Draw", "LiveView")

    menu.set("Movie", "RAW video", "ON")
    menu.set("RAW video", "Resolution", 1920) 
    menu.set("RAW video", "Data format", "12-bit lossless")
    menu.set("RAW video", "Preview", "Auto") 
    camera.shutter.value = 1/50
    camera.iso.value=800
    menu.close()
