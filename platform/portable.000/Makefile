# For portable binaries, without any camera-specific includes
# Will compile autoexec.bin from reboot.c & friends (or ML_SRC_REBOOT_OBJ)

MODEL=portable
FW_VERSION=000
ROMBASEADDR=0xFF000000
RESTARTSTART=0

# this would compile the "full" ML; we don't have any stubs/consts to build it
PRINT_BUILD_INFO = n

include ../Makefile.platform

# dummy definitions to avoid building regular ML
# (we only want what's in reboot.c)

magiclantern.bin:
	@echo Not building magiclantern.bin

magiclantern:
	@echo Not building magiclantern

magiclantern.sym:
	touch magiclantern.sym

cache_d6.o: $(PLATFORM_DIR)/cache_d6.c
	$(call build,CC,$(CC) $(CFLAGS) -march=armv7-r -c -o $@ $<)
