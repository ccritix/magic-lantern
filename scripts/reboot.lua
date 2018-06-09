-- Reboot

function main()
    camera.reboot()
end

keymenu = menu.new
{
    parent = "Modules",
    name = "Reboot",
    help = "Reboots cam. Will not ask for confirmation!",
    select = function(this) task.create(main) end,
}


-- Script from Walter Schultz
-- https://www.magiclantern.fm/forum/index.php?topic=17027.msg165295#msg165295