
# this is a very simple makefile for modules
TOP_DIR=../..

# define the module name - make sure name is max 8 characters
MODULE_NAME=snd_viz
MODULE_OBJS=snd_viz.o kiss_fft.o

# include modules environment
include $(TOP_DIR)/modules/Makefile.modules

CFLAGS += -DFIXED_POINT=32
