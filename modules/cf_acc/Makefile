
# define the module name - make sure name is max 8 characters
MODULE_NAME=cf_acc
MODULE_OBJS=cf_acc.o hdparm/identify.o hdparm/hdparm-routines.o hdparm-ml-shim.o

# include modules environment
include ../Makefile.modules

LINUX_PATH=$(HOME)/src/linux-3.19

CFLAGS += -Ihdparm
CFLAGS += -Idietlibc/include
CFLAGS += -I$(LINUX_PATH)/arch/arm/include/uapi
CFLAGS += -I$(LINUX_PATH)/arch/arm/include
CFLAGS += -I$(LINUX_PATH)/include/uapi
CFLAGS += -I$(LINUX_PATH)/include

# fixme: properly handle generic-y from Linux makefiles
# (hack: copy and rename folders around manually)
CFLAGS += -I$(LINUX_PATH)/generic

test: FORCE
	gcc hdparm-test.c hdparm-ml-shim.c hdparm/identify.c hdparm/hdparm-routines.c -Ihdparm -o hdparm-test
	./hdparm-test
