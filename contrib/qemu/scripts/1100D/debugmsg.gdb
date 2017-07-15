# ./run_canon_fw.sh 1100D -d debugmsg
# ./run_canon_fw.sh 1100D -d debugmsg -s -S & arm-none-eabi-gdb -x 1100D/debugmsg.gdb

source -v debug-logging.gdb

macro define CURRENT_TASK 0x1a2c
macro define CURRENT_ISR  (*(int*)0x670 ? (*(int*)0x674) >> 2 : 0)

# GDB hook is very slow; -d debugmsg is much faster
# ./run_canon_fw.sh will use this address, don't delete it
# b *0xFF06C914
# DebugMsg_log

b *0xFF06FAF4
task_create_log

b *0xFF1D34CC
SetEDmac_log

continue
