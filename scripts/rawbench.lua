--[[

Raw video benchmark, unattended.
================================

WARNING: this script AUTORUNS, FORMATS THE CARD
and REBOOTS THE CAMERA without user confirmation!
Be careful!

What it does:
- Formats the card (simulated keypresses in Canon menu)
- Picks a random raw_rec version from ML/MODULES/RAW_REC/*.mo
- Restarts the camera to load the new module
- Records 10 test clips with current settings
- Writes number of frames from each clip to a log file
- Also writes some basic statistics (median, Q1, Q3)
- Repeats over and over

]]--

require("logger")
require("stats")

-- global logger
log = nil

function string.starts(String,Start)
   return string.sub(String,1,string.len(Start))==Start
end

function string.ends(String,End)
   return End=='' or string.sub(String,-string.len(End))==End
end

function check_card_empty()
    local dir = dryos.dcim_dir
    assert(dir.exists)
    
    for i,filename in pairs(dir:files()) do
        return false
    end
    
    return true
end

-- this does not perform any checks,
-- just reads 4 bytes from footer
function extract_num_frames(raw_filename)
    local f = io.open(raw_filename, "r")
    f:seek("end", -0xB4)
    -- fixme: nicer way to read an integer from a binary file?
    local lo = string.byte(f:read(1))
    local hi = string.byte(f:read(1))
    local LO = string.byte(f:read(1))
    local HI = string.byte(f:read(1))
    f:close()
    return lo + hi * 256 + LO * 65536 + HI * 65536 * 256
end

function find_last_chunk(raw_filename)
    local last = raw_filename
    for i = 0,10 do
        local chunk_filename = string.sub(raw_filename, 1, -4) 
            .. string.format("R%02d", i)
        local f = io.open(chunk_filename, "rb")
        if f then
            last = chunk_filename
            f:close()
        else
            break
        end
    end
    return last
end

function get_num_frames(raw_filename)
    local last_chunk = find_last_chunk(raw_filename)
    local num_frames = extract_num_frames(last_chunk)
    return num_frames, last_chunk
end


function check_files()
    print("Checking clips...")
    local dir = dryos.dcim_dir
    assert(dir.exists)
    local nframes_list = {}

    -- todo: MLV support
    for i,filename in pairs(dir:files()) do
        if filename:ends(".RAW") then
            local num_frames, last_chunk, ok, err
            ok, num_frames, last_chunk =
                xpcall(get_num_frames, debug.traceback, filename)
            if ok then
                log:writef("%s: %s\n", last_chunk, num_frames)
                table.insert(nframes_list, num_frames)
            else
                log:write(num_frames) -- log traceback
            end
        end
    end
    
    local q1, median, q3, _ = stats.quartiles(nframes_list)
    
    log:writef("Recorded frames : %s\n", table.concat(nframes_list, ","))
    log:writef("Quartile summary: %s frames (%s...%s)\n", median, q1, q3)
end

-- note: this is fragile, requires precise initial conditions:
-- LiveView, and Format already pre-selected in Canon menu
function format_card()
    if camera.model_short == "5D3" then
        print("Formatting card...")
        assert(lv.enabled)      -- from LiveView,
        key.press(KEY.MENU)     -- open Canon menu
        msleep(1000)
        assert(not lv.enabled)
        print("Please select the Format menu item.")
        print("Do not click it, just move the selection bar.")
        -- fixme: io.write
        console.write("The script will press SET in: ")
        for i = 20,0,-1 do
            console.write(string.format("%d... ", i))
            msleep(1000)
        end
        print ""
        key.press(KEY.SET)      -- assuming we are on the Format item, press SET
        msleep(500)
        key.press(KEY.LEFT)     -- press LEFT to select CF card
        key.press(KEY.UNPRESS_UDLR)
        msleep(200)
        key.press(KEY.SET)      -- confirm selection
        msleep(500)             -- wait for ML to copy itself in RAM
        key.press(KEY.RIGHT)    -- press RIGHT to select OK
        key.press(KEY.UNPRESS_UDLR)
        msleep(200)
        key.press(KEY.SET)      -- press OK
        msleep(10000)           -- wait until format is complete
        key.press(KEY.MENU)     -- close Canon menu
        msleep(1000)
        assert(lv.enabled)      -- back to LiveView?
        print("Formatting complete (hopefully)")
    else
        print("Please edit the script to support your camera.")
    end
end

function random_raw_rec()
    local dir = dryos.directory("ML/MODULES/RAW_REC/")
    assert(dir.exists)
    local files = {}
    for i,filename in pairs(dir:files()) do
        if filename:ends(".MO") then
            table.insert(files, filename)
        end
    end
    assert(#files)
    local selected_filename = files[math.random(1, #files)]
    log:writef("Raw_rec version : %s\n", selected_filename)
    
    -- will this copy without errors? we'll see
    local fin = io.open(selected_filename, "rb")
    local data = fin:read("*all")
    fin:close()

    local fout = io.open("ML/MODULES/RAW_REC.MO", "w")
    fout:write(data)
    fout:close()

    -- verify
    fin = io.open("ML/MODULES/RAW_REC.MO", "rb")
    local check = fin:read("*all")
    fin:close()
    assert(data == check)
    
    -- reboot needed to load the new module
end

function run_test()

    if not menu.get("Movie", "RAW video") and
       not menu.get("Movie", "RAW video (MLV)") then
       print("Please enable RAW video recording.")
       return
    end
    
    if not check_card_empty() then
        print("DCIM directory is not empty.")
        print("Going to format the card, then record 10 test clips,")
        print("then pick a random raw_rec from ML/MODULES/RAW_REC/")
        print("====================================================")
        print("")
        msleep(5000)

        format_card()
        random_raw_rec()
        
        print("Restarting...")
        msleep(5000)
        camera.reboot()
        return
    else
        print("DCIM directory is empty.")
        print("Going to record 10 test clips.")
        print("=============================")
        print("")
    end
    
    print("About to start recording...")
    msleep(5000)
    console.hide()
    
    for i = 1,10 do
        print(string.format("Recording test clip #%d...\n", i))
        movie.start()
        while movie.recording do
            msleep(10000)
        end
        print("Recording stopped.")
        msleep(5000)
    end
    
    -- fixme: this may crash
    local ok, err = xpcall(check_files, debug.traceback)
    if not ok then
        log:write(err)
    end

end

function main()
    math.randomseed(dryos.date.min * 13 + dryos.date.sec + dryos.ms_clock)
    msleep(10000)
    menu.close()
    console.show()
    console.clear()
    
    -- place the log in ML/LOGS so it can survive after format
    log = logger("ML/LOGS/RAWBENCH.LOG")

    run_test()

    log:close()
  
    console.show()
    print("Finished.")
    msleep(2000)

    print("A little break, then... reboot.")
    msleep(60000)
    camera.reboot()
end

--[[
keymenu = menu.new
{
    name   = "Raw video benchmark",
    help   = "Compares different versions of raw_rec, randomly chosen from",
    help2  = "ML/MODULES/RAW_REC/*.MO. Logs number of frames for each test run.",
    select = function(this) task.create(main) end,
}
]]--

task.create(main)
