
# download libmad and place it in a subfolder. put the folder name below (or use exact that one)
MAD_DIR=libmad-0.15.1b
MAD_OUT=$(abspath $(MAD_DIR)/../libmad)

MAD_OBJ=$(MAD_OUT)/lib/libmad.a
MAD_INC=$(MAD_OUT)/include

# define the module name - make sure name is max 8 characters
MODULE_NAME=mp3_play
MODULE_OBJS=mp3_play.o $(MAD_OBJ)


# include modules environment
TOP_DIR=../..
include $(TOP_DIR)/modules/Makefile.modules

CFLAGS += -I$(MAD_INC)

# this is for calling ./configure of libmad
export PATH := $(ARM_PATH):$(PATH)

# configure is a bit picky regarding exact platform type and how you specify it. hope it works for you too :)
$(MAD_DIR)/Makefile: 
	cd $(MAD_DIR) && ./configure --disable-debugging --prefix=$(MAD_OUT) --host=arm-$(ARM_ABI) --disable-shared CFLAGS=--specs=nosys.specs CC=$(CC)
	cd $(MAD_DIR) && echo "\r\n/* redirect malloc/free calls to internal code as ML doesnt provide those directly */\r\n#define malloc mp3_play_malloc\r\n#define free mp3_play_free\r\n" >> config.h

$(MAD_OBJ): $(MAD_DIR)/Makefile
	cd $(MAD_DIR) && make && make install
    
libmad: $(MAD_OBJ)
    
libmad-clean:
	-cd $(MAD_DIR) && make clean
	-rm $(MAD_DIR)/Makefile
	rm -Rf $(MAD_OUT)

mp3_play.o: libmad

clean:: libmad-clean