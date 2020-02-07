--[[
DOFI
Depth of Feild Info script
This script dynamically displays (in the ML top bar) the current infinity defocus blur, diffraction blur and the total blur at infinity.
The script visually shows the overlap status to aid focus bracketing: green means a positive overlap and red a focus gap between the current focus and the last image taken.
Yellow means the current focus is the same as the last image captured.
Changing aperature or focal length will reset DOFI.
Also exposing the ML menu will reset DOFI.
The ML CoC (total blur) is used as the focus overlap criterion.
A thin lens model is assumed, with the lens principal plane positioned at f(1+m) from the sensor, ie magnification is estimated from the TL model: so don't use the script for macro.
Release 0.625
Jan 2020
Garry George
photography.grayheron.net
--]]

require("config")
infinity = 655000 -- trap ML reported infinity
f = lens.focal_length
x = lens.focus_distance 
a = camera.aperture.value
ML_blur = menu.get("DOF Settings","Circle of Confusion",0) -- this is the overlap blur
H = 0
m = 0
r = 0
last_H = H
ndof = 0
fdof = 0
infinity_blur = 0
diff_blur = 0
total_blur = 0
blurs = ""
num = 0
u = 0
last_f = f
last_x = x
last_a = a
last_ndof = ndof
last_fdof = fdof
last_lens_0 = 0
lens_0 = 0
card_count = dryos.shooting_card.file_number
CON = "ON"
COFF = "OFF"
image_taken = false -- that is at least one has been taken
show = true
freq = 0.550 -- 0.550 for visible band photography or 0.850 for IR (or another frequency to suit your sensor conversion)

function my_display(text,time,x_pos,y_pos)
    text = string.rep("\n",y_pos)..string.rep(" ",x_pos)..text
    display.notify_box(text,time*1000)
end

if (x==0 or a==0 or f==0) then 
    my_display("Can't use DOFI",2,0,0)
    return
end

function myround(nu) return tostring(math.floor(nu+0.5)) end

function update()
    if f ~= last_f or a ~= last_a or menu.visible then -- reset
        image_taken = false 
        card_count = dryos.shooting_card.file_number
        last_f = lens.focal_length
        last_a = camera.aperture.value
    end
    
    if card_count ~= dryos.shooting_card.file_number then
        last_f = f
        last_x = x
        last_lens_0 = lens_0
        last_a = a
        last_H = H
        last_ndof = ndof
        last_fdof = fdof
        card_count = dryos.shooting_card.file_number
        image_taken = true
    end
    
    f = lens.focal_length
    ML_blur = menu.get("DOF Settings","Circle of Confusion",0)
    x = lens.focus_distance -- supplied by Canon from sensor
    if x < infinity then 
        r = math.sqrt((x*x)/4 - f*x)
        m = (x - 2*r)/(x + 2*r)
    else
        m = 0
    end
    lens_0 = f * (1.0 + m)

    u = x - lens_0
    a = camera.aperture.value
    H = f + (1000*f*f)/(a*ML_blur)
    ndof = (H*u-f*f)/(H+u-2*f)
    if u < H then 
        fdof = (H*u-2*f*u+f*f)/(H-u)
    else 
        fdof = infinity
    end

    num = math.ceil((1 + (H/u))/2) -- estimate of number of focus brackets
    if u > H then num = 1 end

    infinity_blur = ML_blur*(H-f)/(x-lens_0-f)
    diff_blur = 2.44 * a * freq 
    total_blur = math.sqrt(infinity_blur*infinity_blur + diff_blur*diff_blur)
    blurs = myround(infinity_blur).."/"..myround(diff_blur).."/"..myround(total_blur)
    if x > infinity then
        blurs = "INF: "..blurs
    else
        blurs = "#"..num..": "..blurs
    end
end

function check_stuff()
    update()
    return true
end

DOFI_Menu = menu.new
{
    parent = "Focus",
    name = "DOF Info",
    help = "Helps with infinity focusing & focus bracketing",
    depends_on = DEPENDS_ON.LIVEVIEW,
    choices = {CON,COFF},
    update = function(this)
        if this.value == "ON" then 
            show = true
        else 
            show = false
        end
    end,
}

lv.info
{
    name = "DOFI",
    value = "",
    priority = 100,
    update = function(this)
        if (not show) then 
            this.value = ""
        else
            this.value = blurs
            this.background = COLOR.WHITE
            this.foreground = COLOR.BLACK 
            if image_taken then
                if (x == last_x) then
                    this.background = COLOR.YELLOW
                    this.foreground = COLOR.BLACK
                elseif (ndof > last_fdof) or (fdof < last_ndof) then
                    this.background = COLOR.RED
                    this.foreground = COLOR.WHITE 
                elseif (ndof < last_ndof) and (fdof > last_ndof) then
                    this.background = COLOR.GREEN1
                    this.foreground = COLOR.BLACK 
                elseif (ndof > last_ndof) and (ndof <= last_fdof) then
                    this.background = COLOR.GREEN1
                    this.foreground = COLOR.BLACK 
                end

                if x > infinity then 
                    this.background = COLOR.BLACK
                    this.foreground = COLOR.WHITE
                end
            else
                if x > infinity then 
                    this.value = blurs
                    this.background = COLOR.BLACK
                    this.foreground = COLOR.WHITE
                end
            end
        end
    end
}

event.shoot_task = check_stuff

config.create_from_menu(DOFI_Menu) -- keep a track of the script's menu state at camera close