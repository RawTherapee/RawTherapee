include (libraw-common.pro)
win32:LIBS+=libraw.lib
unix:LIBS+=-lraw
CONFIG-=qt
CONFIG+=debug_and_release
SOURCES=../samples/mem_image_sample.cpp
