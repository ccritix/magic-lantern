
# define the module name - make sure name is max 8 characters
MODULE_NAME=raw_rec
MODULE_OBJS=raw_rec.o

# include modules environment
include ../Makefile.modules

all:: raw2dng

# RAW to DNG converter for PC
raw2dng: ../lv_rec/raw2dng.c
	$(call build,HOST_CC,$(HOST_CC) -c ../dng/dng.c -std=gnu99 -O2 -Wall -I$(SRC_DIR))
	$(call build,HOST_CC,$(HOST_CC) -c ../lv_rec/raw2dng.c -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,HOST_CC,$(HOST_CC) raw2dng.o dng.o -o raw2dng -lm)

# debug tool
dng2raw: dng2raw.c
	$(call build,GCC,gcc dng2raw.c -O2 -Wall -I$(SRC_DIR)) -o dng2raw

raw2dng.exe: $(SRC_DIR)/chdk-dng.c ../lv_rec/raw2dng.c
	$(call build,MINGW,$(MINGW_GCC) -c $(SRC_DIR)/chdk-dng.c -m32 -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR))
	$(call build,MINGW,$(MINGW_GCC) -c ../lv_rec/raw2dng.c -m32 -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,MINGW,$(MINGW_GCC) raw2dng.o chdk-dng.o -o raw2dng.exe -lm -m32)

raw2dng.exe: ../lv_rec/raw2dng.c
	$(call build,MINGW,$(MINGW_GCC) -c ../dng/dng.c -O2 -Wall -I$(SRC_DIR))
	$(call build,MINGW,$(MINGW_GCC) -c ../lv_rec/raw2dng.c -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,MINGW,$(MINGW_GCC) raw2dng.o dng.o -o raw2dng.exe -lm -m32)

clean::
	$(call rm_files, raw2dng raw2dng.exe dng2raw dng2raw.exe)
