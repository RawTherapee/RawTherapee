win32:CONFIG+=console
win32:LIBS+=libraw.lib
unix:LIBS+=-lraw
win32-g++:
{
    LIBS += -lws2_32
}
include (libraw-common-lib.pro)