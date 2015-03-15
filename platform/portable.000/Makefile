# For portable binaries, without any camera-specific includes
# Will compile autoexec.bin from reboot.c & friends (or ML_SRC_REBOOT_OBJ)

MODEL=portable
FW_VERSION=000
ROMBASEADDR=0xFF000000
RESTARTSTART=0
include ../Makefile.platform

# dummy definitions to avoid building regular ML
# (we only want what's in reboot.c)

magiclantern.bin:
	@echo Not building magiclantern.bin

magiclantern:
	@echo Not building magiclantern

magiclantern.sym:
	touch magiclantern.sym
