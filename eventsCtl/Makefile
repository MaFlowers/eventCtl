SHLIB = libeventctl.so
#SLIB = libeventctl.a
TGTDIR = ..

CFLAGS = -I. -I../common -g -fPIC

#LDFLAGS += -L./ -L../Cthread_Pool -lcthread -L../common -lcomm
LDFLAGS += -shared -L./ -L../common -lcomm

include $(TGTDIR)/Makefile.inc
