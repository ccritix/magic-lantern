--[[---------------------------------------------------------------------------
Touchscreen enhancements for ML

Swipe down: open ML menu
Swipe up: close ML menu
Swipe from left/right near top: adjust shutter speed
Swipe from left/right near middle: adjust aperture
Swipe from left/right near bottom: adjust iso
]]

swipe_down_start_y = display.height // 5
swipe_up_start_y = display.height * 4 // 5
swipe_height = display.height // 3
menu_swipe_start_x = display.width // 2
menu_swipe_width = display.width // 3

tv_y1 = 0
tv_y2 = display.height // 3
av_y1 = display.height // 3
av_y2 = display.height * 2 // 3
iso_y1 = display.height * 2 // 3
iso_y2 = display.height
swipe_right_x = display.width // 12
swipe_left_x = display.width * 11 // 12
swipe_horizontal_width = display.width // 12
settings_adjust = 0
start_x = 0
start_y = 0
iso_start = 0
av_start = 0
tv_start = 0
move_x = 0

menu.block_touch(true)

touch.down = function(x,y,id)
    start_x = x
    start_y = y
    if menu.visible == false and lv.enabled and (x > swipe_left_x or x < swipe_right_x) and y > tv_y1 and y < iso_y2 then
        local move_x = x // swipe_horizontal_width
        local move_dir = 1
        if x > swipe_left_x then move_dir = -1 end
        if y > iso_y1 then 
            settings_adjust = 3
            iso_start = camera.iso.apex
        elseif y > av_y1 then 
            settings_adjust = 2
            av_start = camera.aperture.apex
        else 
            settings_adjust = 1 
            tv_start = camera.shutter.apex
        end
        touch.move = function(mx,my,mid)
            if mx // swipe_horizontal_width ~= move_x then
                move_x = mx // swipe_horizontal_width
                local move_amt = move_x / 3
                if move_dir == -1 then move_amt = (move_x - 12) / 3 end
                if settings_adjust == 1 then
                    camera.shutter.apex = tv_start - move_amt
                    display.notify_box(tostring(camera.shutter))
                elseif settings_adjust == 2 then
                    camera.aperture.apex = av_start - move_amt
                    display.notify_box(tostring(camera.aperture))
                elseif settings_adjust == 3 then
                    camera.iso.apex = iso_start + move_amt
                    display.notify_box(tostring(camera.iso))
                end
            end
        end
    end
end

touch.up = function(x,y,id)
    if settings_adjust > 0 then
        touch.move = nil
        settings_adjust = 0
    end
    if menu.visible == false and start_y < swipe_down_start_y and y - start_y > swipe_height then
        menu.open()
    elseif menu.visible then
        if start_y > swipe_up_start_y and start_y - y > swipe_height then
            menu.close()
        elseif start_x > menu_swipe_start_x and start_x - x > menu_swipe_width then
            menu.close_submenu()
        elseif start_x < menu_swipe_start_x and x - start_x > menu_swipe_width then
            menu.open_submenu()
        end
    end
end