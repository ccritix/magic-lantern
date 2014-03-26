
# define the module name - make sure name is max 8 characters
MODULE_NAME=raw_rec
MODULE_OBJS=raw_rec.o

ML_DNG_DIR=../ml_dng

# include modules environment
include ../Makefile.modules

all:: raw2dng

# RAW to DNG converter for PC
raw2dng: $(ML_DNG_DIR)/ml_dng.c ../lv_rec/raw2dng.c
	$(call build,GCC,gcc -c $(ML_DNG_DIR)/ml_dng.c -m32 -O2 -Wall -I$(SRC_DIR) -I$(ML_DNG_DIR))
	$(call build,GCC,gcc -c ../lv_rec/raw2dng.c -m32 -O2 -Wall -I$(SRC_DIR) -I$(ML_DNG_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,GCC,gcc raw2dng.o ml_dng.o -o raw2dng -lm -m32)

# debug tool
dng2raw: dng2raw.c
	$(call build,GCC,gcc dng2raw.c -m32 -O2 -Wall -I$(SRC_DIR)) -o dng2raw

MINGW=~/mingw-w32/bin/i686-w64-mingw32-gcc

raw2dng.exe: $(ML_DNG_DIR)/ml_dng.c ../lv_rec/raw2dng.c
	$(call build,MINGW,$(MINGW) -c $(ML_DNG_DIR)/ml_dng.c -m32 -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -I$(ML_DNG_DIR))
	$(call build,MINGW,$(MINGW) -c ../lv_rec/raw2dng.c -m32 -mno-ms-bitfields -O2 -Wall -I$(SRC_DIR) -I$(ML_DNG_DIR) -D_FILE_OFFSET_BITS=64)
	$(call build,MINGW,$(MINGW) raw2dng.o ml_dng.o -o raw2dng.exe -lm -m32)

clean::
	$(call rm_files, raw2dng)
