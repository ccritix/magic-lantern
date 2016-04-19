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
- Records NUM_CLIPS test clips with current settings
- Writes number of frames from each clip to a log file
- Also writes some basic statistics (median, Q1, Q3)
- Repeats over and over

]]--

require("logger")
require("stats")

-- number of clips
NUM_CLIPS = 8

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
function extract_num_frames_raw(raw_filename)
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

function find_last_chunk_raw(raw_filename)
    local last = raw_filename
    for i = 0,99 do
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

function get_num_frames_raw(raw_filename)
    local last_chunk = find_last_chunk_raw(raw_filename)
    local num_frames = extract_num_frames_raw(last_chunk)
    return num_frames, last_chunk
end

function read_next_block_mlv(file)
    -- fixme: nicer way to read an integer from a binary file?
    local block_type = file:read(4)
    local lo = string.byte(file:read(1))
    local hi = string.byte(file:read(1))
    local LO = string.byte(file:read(1))
    local HI = string.byte(file:read(1))
    local block_size = lo + hi * 256 + LO * 65536 + HI * 65536 * 256
    -- seek to next block
    file:seek("cur", block_size - 8)
    return block_type
end

function extract_num_frames_mlv(mlv_filename)
    local f = io.open(mlv_filename, "r")
    if f == nil then
        return -1
    end
    
    -- fixme: these return int32, not good above 2GB
    -- workaround: incremental seeks until it fails
    local num_frames = 0
    while true do
    
        local ok, block_type = pcall(read_next_block_mlv, f)
        
        if not ok then
            break
        end
        
        if block_type == "VIDF" then
            num_frames = num_frames + 1
        end
    end
    f:close()
    return num_frames
end

function get_num_frames_mlv(mlv_filename)
    
    local last_chunk = mlv_filename
    local num_frames = extract_num_frames_mlv(mlv_filename)

    for i = 0,99 do
        local chunk_filename = string.sub(mlv_filename, 1, -4) 
            .. string.format("M%02d", i)
        
        local num_frames_chunk = extract_num_frames_mlv(chunk_filename)
        
        if num_frames_chunk > 0 then
            num_frames = num_frames + num_frames_chunk
            last_chunk = chunk_filename
        else
            break
        end
    end

    return num_frames, last_chunk
end

function get_num_frames(filename)
    if filename:ends(".RAW") then
        return get_num_frames_raw(filename)
    elseif filename:ends(".MLV") then
        return get_num_frames_mlv(filename)
    end
end

function check_files()
    print("Checking clips...")
    local dir = dryos.dcim_dir
    assert(dir.exists)
    local nframes_list = {}

    -- todo: MLV support
    for i,filename in pairs(dir:files()) do
        if filename:ends(".RAW") or filename:ends(".MLV") then
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
    if camera.model_short == "5D3" or 
       camera.model_short == "5D2" or
       camera.model_short == "50D" or
       camera.model_short == "60D" or
       camera.model_short == "6D" or
       camera.model_short == "7D"
    then
        print("Formatting card...")
        assert(lv.enabled)      -- from LiveView,
        key.press(KEY.MENU)     -- open Canon menu
        msleep(1000)
        assert(not lv.enabled)
        print("Please select the Format menu item.")
        print("Do not click it, just move the selection bar.")
        -- fixme: io.write
        console.write("The script will press SET in NN seconds...")
        for i = 20,0,-1 do
            console.write(string.format("\b\b\b\b\b\b\b\b\b\b\b\b\b%2d seconds...", i))
            msleep(1000)
        end
        print ""
        print "Formatting..."
        key.press(KEY.SET)      -- assuming we are on the Format item, press SET
        if camera.model_short == "5D3" then
            msleep(200)
            key.press(KEY.LEFT)     -- press LEFT to select CF card
            key.press(KEY.UNPRESS_UDLR)
            msleep(200)
            key.press(KEY.SET)      -- confirm selection
        end
        msleep(5000)            -- wait for ML to copy itself in RAM
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
       not menu.get("Movie", "RAW video (MLV)")
    then
       print("Please enable RAW video recording.")
       return
    end
    
    if not check_card_empty() then
        -- count frames from existing files, if any
        -- fixme: this may crash
        local ok, err = xpcall(check_files, debug.traceback)
        if not ok then
            log:write(err)
        end
        print("To stop the experiment, exit LiveView.")
        msleep(5000)
        if not lv.enabled then
            print("Experiment stopped (not in LiveView)")
            return
        end

        print("=====================================================")
        print("DCIM directory is not empty.")
        print("Going to format the card, then record the test clips,")
        print("then pick a random raw_rec from ML/MODULES/RAW_REC/ .")
        print("=====================================================")
        msleep(5000)

        format_card()
        random_raw_rec()
        
        print("Restarting...")
        msleep(5000)
        camera.reboot()
        return
    else
        print("DCIM directory is empty.")
        print(string.format("Going to record %d test clips.", NUM_CLIPS))
        print("==============================")
        print("")
    end
    
    print("About to start recording...")
    msleep(5000)
    
    for i = 1,NUM_CLIPS do
        print(string.format("Recording test clip #%d/%d...\n", i, NUM_CLIPS))
        msleep(2000)
        console.hide()

        movie.start()
        while movie.recording do
            msleep(10000)
        end
        
        console.show()
        print("Recording stopped.")
        
        -- wait for all frames to be saved
        msleep(20000)
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

    if lv.enabled then
        print("A little break, then... reboot.")
        msleep(60000)
        camera.reboot()
    end
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
