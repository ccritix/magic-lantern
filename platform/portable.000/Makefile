# For portable binaries, without any camera-specific includes
# Will compile autoexec.bin from reboot.c & friends (or ML_SRC_REBOOT_OBJ)

MODEL=portable
FW_VERSION=000
ROMBASEADDR=0xFF000000
RESTARTSTART=0

# force execution in uncacheable memory (flag 0x40000000)
# Canon code jumps to AUTOEXEC.BIN / *.FIR at 0x800000 / 0x800120
AUTOEXEC_BASE = 0x40800000
FIR_BASE      = 0x40800120

include ../Makefile.platform

# dummy definitions to avoid building regular ML
# (we only want what's in reboot.c)

magiclantern.bin:
	@echo Not building magiclantern.bin

magiclantern:
	@echo Not building magiclantern

magiclantern.sym:
	touch magiclantern.sym
