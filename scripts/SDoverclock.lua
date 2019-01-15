-- enable SD overclocking

  console.hide()
  menu.close()
if menu.get("Debug", "SD overclock", "MAY CAUSE DATA LOSS") == "MAY CAUSE DATA LOSS" then
  if menu.get("Movie", "RAW video", "") == "OFF" then
    display.notify_box("enabling of SD overclocking")
    menu.set("Debug", "SD overclock", "ON")
    msleep(1000)
    display.notify_box("patching...")
    msleep(2000)
    display.notify_box("done!")
    msleep(1000)
  else
    menu.set("Movie", "RAW video", "OFF")
    display.notify_box("enabling of SD overclocking")
    menu.set("Debug", "SD overclock", "ON")
    msleep(1000)
    display.notify_box("patching...")
    msleep(3000)
    menu.select("Movie", "RAW video")
    menu.open()     -- open ML menu
    key.press(KEY.SET)
    menu.close()
    display.notify_box("done!")
    msleep(1000)
  end
else
  display.notify_box("Please enable sd_uhs.mo")
  msleep(2000)
  display.notify_box("Please enable sd_uhs.mo")
  msleep(2000)
  return
end

