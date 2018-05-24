-- enable SD overclocking

    if menu.get("Debug", "SD overclock", "OFF") 
    then
    menu.set("Debug", "SD overclock", "ON")
    else
    console.show()
    print("Please enable sd_uhs.mo before running this script again.")
    msleep(5000)
    console.hide()
    return
    end