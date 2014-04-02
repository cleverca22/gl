CC=gcc
CXX=g++
LD=ld
CFLAGS=-O3 -g -Wdeclaration-after-statement -I/media/videos/1tb/rpi/hackdriver/ -I/opt/vc/include/interface/vmcs_host/linux/ -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/
LDFLAGS=-L/opt/vc/lib/ -L/media/videos/1tb/rpi/lib -lbcm_host -lc -lV3D2

OBJECTS=core.o mailbox.o
all: libGL.so test1 texture.raw dispmanx_snapshot
libGL.so: ${OBJECTS}
	${LD} -o $@ $+ -shared ${LDFLAGS}
	cp -v $@ /media/videos/1tb/rpi/lib/
test1: test1.o core.o mailbox.o
	${CXX} -o $@ $+ ${LDFLAGS}
dispmanx_snapshot: dispmanx_snapshot.o
	${CXX} -o $@ $+ ${LDFLAGS}
core.o: core.c mailbox.h
mailbox.o: mailbox.cpp mailbox.h

%.o: %.c
	${CC} -c -o $@ $< ${CFLAGS}

%.raw: %.s
	../videocoreiv-qpu/qpu-tutorial/qpuasm.js --raw=$@ $+
