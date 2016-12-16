
# define the module name - make sure name is max 8 characters
MODULE_NAME=raw_rec
MODULE_OBJS=raw_rec.o

# include modules environment
include ../Makefile.modules

R2D_CFLAGS = -I$(SRC_DIR) -mno-ms-bitfields -D_FILE_OFFSET_BITS=64
R2D_LFLAGS = -lm

# RAW to DNG converter for PC
raw2dng: ../dng/dng.c ../lv_rec/raw2dng.c
	$(call build,HOST_CC,$(HOST_CC) -c ../dng/dng.c $(HOST_CFLAGS) $(R2D_CFLAGS))
	$(call build,HOST_CC,$(HOST_CC) -c ../lv_rec/raw2dng.c $(HOST_CFLAGS) $(R2D_CFLAGS))
	$(call build,HOST_CC,$(HOST_CC) raw2dng.o dng.o -o raw2dng $(HOST_LFLAGS) $(R2D_LFLAGS))

# debug tool
dng2raw: dng2raw.c
	$(call build,HOST_CC,$(HOST_CC) dng2raw.c $(HOST_CFLAGS) $(R2D_CFLAGS)) -o dng2raw

raw2dng.exe: ../dng/dng.c ../lv_rec/raw2dng.c
	$(call build,MINGW,$(MINGW_GCC) -c ../dng/dng.c $(HOST_CFLAGS) $(R2D_CFLAGS))
	$(call build,MINGW,$(MINGW_GCC) -c ../lv_rec/raw2dng.c $(HOST_CFLAGS) $(R2D_CFLAGS))
	$(call build,MINGW,$(MINGW_GCC) raw2dng.o dng.o -o raw2dng.exe $(HOST_LFLAGS) $(R2D_LFLAGS))

dng2raw.exe: dng2raw.c
	$(call build,MINGW,$(MINGW_GCC) dng2raw.c $(HOST_CFLAGS) $(R2D_CFLAGS)) -o dng2raw.exe

clean::
	$(call rm_files, raw2dng raw2dng.exe dng2raw dng2raw.exe)
