
# define the module name - make sure name is max 8 characters
MODULE_NAME=lv_rec
MODULE_OBJS=lv_rec.o

# include modules environment
include ../Makefile.modules

all:: raw2dng


# RAW to DNG converter for PC
raw2dng: FORCE
	$(call build,HOST_CC,$(HOST_CC) -c ../dng/dng.c -std=gnu99 -O2 -Wall -I$(SRC_DIR))
	$(call build,HOST_CC,$(HOST_CC) -c raw2dng.c -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,HOST_CC,$(HOST_CC) raw2dng.o dng.o -o raw2dng -lm)

raw2dng.exe: FORCE
	$(call build,MINGW,$(MINGW_GCC) -c ../dng/dng.c -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR))
	$(call build,MINGW,$(MINGW_GCC) -c raw2dng.c -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,MINGW,$(MINGW_GCC) raw2dng.o dng.o -o raw2dng.exe -lm)

clean::
	$(call rm_files, raw2dng raw2dng.exe)
