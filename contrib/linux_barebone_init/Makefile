
CPATH=/usr/bin
CPREFIX=arm-linux-gnueabi-
#CPATH=/root/gcc-arm-none-eabi-4_9-2015q1/bin
#CPREFIX=arm-none-eabi-
#SYSROOT=/root/busybox/arm-linux-uclibc

CC=$(CPATH)/$(CPREFIX)gcc
LD=$(CPATH)/$(CPREFIX)ld

# lib objects to link against statically
#LIBS=crt1.o crti.o

# this executable's objects
OBJECTS=init.o

CFLAGS=-Wall -I$(SYSROOT)/usr/include -march=armv5 -mlittle-endian -marm -r -fPIC
ASLAGS=
LDFLAGS=-elf2flt="-v"
#-Wl,-rpath-link,$(SYSROOT)/usr/lib/ $(foreach lib,$(LIBS),$(SYSROOT)/usr/lib/$(lib))

.PHONY: all clean distclean
all: init
#	mount /root/initrd.img /mnt
#	cp init /mnt/bin/init
#	umount /mnt

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@ 

init: $(OBJECTS)
	$(LD) $(LDFLAGS) $< -o $@

clean:
	@- $(RM) init
	@- $(RM) $(OBJECTS)

distclean: clean
