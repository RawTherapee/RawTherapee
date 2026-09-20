include (libraw-common.pro)
win32:LIBS+=libraw.lib
unix:LIBS+=-lraw
CONFIG-=qt
SOURCES=../samples/4channels.cpp
