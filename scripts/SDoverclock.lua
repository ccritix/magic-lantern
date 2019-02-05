-- sd overclock engine

-- Works with sd_uhs.mo enabled 

  console.hide()
  msleep(200) -- let´s start off with a nap
if menu.get("Debug", "SD overclock", "MAY CAUSE DATA LOSS") == "MAY CAUSE DATA LOSS" then
  if menu.get("Movie", "RAW video", "") == "OFF" then
 display.notify_box("SD overclocking, please wait...")
    menu.set("Debug", "SD overclock", "ON")
    msleep(1000)
 display.notify_box("patched")
 else
    menu.set("Movie", "RAW video", "OFF")
    msleep(500)
 display.notify_box("SD overclocking, please wait...")
    menu.set("Debug", "SD overclock", "ON")
    msleep(1000)
    menu.set("Movie", "RAW video", "ON")
    menu.select("Movie", "RAW video")
  if menu.get("Movie", "Crop mode", "") ~= "OFF" then
    menu.open()
    msleep(100)
    menu.close()
  end
 display.notify_box("patched")
  end
 else
 display.notify_box("Please enable sd_uhs.mo")
    msleep(2000)
 display.notify_box("Please enable sd_uhs.mo")
    msleep(2000)
    return
end
