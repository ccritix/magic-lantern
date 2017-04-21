
# define the module name - make sure name is max 8 characters
MODULE_NAME=mlv_rec
MODULE_OBJS=mlv_rec.o mlv.o

# include modules environment
include ../Makefile.modules

MLV_CFLAGS = -I$(SRC_DIR) -m32 -Wpadded -mno-ms-bitfields -std=c99 
MLV_LFLAGS = -m32
MLV_LIBS = -lm
MLV_LIBS_MINGW = -lm


# just comment out to disable LUA
#LUA_VER=5.2

ifdef LUA_VER
    MLV_CFLAGS += $(shell pkg-config --cflags lua$(LUA_VER)) -D USE_LUA
    MLV_LFLAGS += 
    MLV_LIBS += $(shell pkg-config --libs lua$(LUA_VER))
    MLV_LIBS_MINGW += $(shell pkg-config --libs lua$(LUA_VER))
endif


# add target architecture for OSX
ifdef FOR_OSX
    MLV_CFLAGS += -mmacosx-version-min=10.6
    MLV_LFLAGS += -mmacosx-version-min=10.6
endif


MLV_CFLAGS +=
MLV_LFLAGS += 
MLV_LIBS +=
MLV_LIBS_MINGW +=


MLV_DUMP_OBJS=mlv_dump.host.o lj92.host.o $(DNG_OBJ) $(RAW_PROC_OBJS)
MLV_DUMP_OBJS_MINGW=mlv_dump.w32.o lj92.w32.o $(DNG_OBJ_MINGW) $(RAW_PROC_OBJS_MINGW)



clean::
	$(call rm_files, mlv_dump mlv_dump.exe $(LZMA_OBJS) $(LZMA_LIB) $(LZMA_OBJS_MINGW) $(LZMA_LIB_MINGW) )

#
# rules for host and win32 objects
#
%.host.o: %.c
	$(call build,HOST_CC,$(HOST_CC) $(HOST_CFLAGS) $(HOST_LFS_FLAGS) $(MLV_CFLAGS) -o $@ -c $<)

%.w32.o: %.c
	$(call build,MINGW_GCC,$(MINGW_GCC) $(MINGW_CFLAGS) $(MINGW_LFS_FLAGS) $(MLV_CFLAGS) -o $@ -c $<)

#

# mlv_dump rules
#
mlv_dump: $(MLV_DUMP_OBJS)
	$(call build,HOST_CC,$(HOST_CC) $(HOST_LFLAGS) $(MLV_LFLAGS) $(MLV_DUMP_OBJS) -o $@ $(HOST_LIBS) $(MLV_LIBS) )

mlv_dump.exe: $(MLV_DUMP_OBJS_MINGW)
	$(call build,MINGW_GCC,$(MINGW_GCC) $(MINGW_LFLAGS) $(MLV_LFLAGS) $(MLV_DUMP_OBJS_MINGW) -o $@ $(MINGW_LIBS) $(MLV_LIBS_MINGW) )

