
# define the module name - make sure name is max 8 characters
MODULE_NAME=mlv_rec
MODULE_OBJS=mlv_rec.o mlv.o

# include modules environment
include ../Makefile.modules

MLV_CFLAGS = -I$(SRC_DIR) -D MLV_USE_LJ92 -m32 -Wpadded -mno-ms-bitfields -D MLV2DNG
MLV_LFLAGS = 
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
    MLV_CFLAGS += -arch i386 -mmacosx-version-min=10.6
    MLV_LFLAGS += -arch i386 -mmacosx-version-min=10.6
endif


MLV_CFLAGS += 
MLV_LFLAGS += 
MLV_LIBS += 
MLV_LIBS_MINGW += 

MLV_DUMP_OBJS=mlv_dump.host.o $(SRC_DIR)/chdk-dng.host.o ../lv_rec/raw2dng.host.o lj92.host.o
MLV_DUMP_OBJS_MINGW=mlv_dump.w32.o $(SRC_DIR)/chdk-dng.w32.o ../lv_rec/raw2dng.w32.o lj92.w32.o


clean::
	$(call rm_files, mlv_dump mlv_dump.exe)

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

