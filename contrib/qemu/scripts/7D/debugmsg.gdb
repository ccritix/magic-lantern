# ./run_canon_fw.sh 7D -d debugmsg
# ./run_canon_fw.sh 7D -d debugmsg -s -S & arm-none-eabi-gdb -x 7D/debugmsg.gdb

source -v debug-logging.gdb

# To get debugging symbols from Magic Lantern, uncomment this:
#symbol-file ../magic-lantern/platform/7D.203/magiclantern

macro define CURRENT_TASK 0x1a1c
macro define CURRENT_ISR  (*(int*)0x670 ? (*(int*)0x674) >> 2 : 0)

# GDB hook is very slow; -d debugmsg is much faster
# ./run_canon_fw.sh will use this address, don't delete it
# b *0xFF0776AC
# DebugMsg_log

b *0xFF07BEAC
task_create_log

b *0xFF205A44
register_interrupt_log

cont
