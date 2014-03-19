CC=gcc
CXX=g++
LD=ld
CFLAGS=-g -Wdeclaration-after-statement -I/media/videos/4tb/hackdriver/ -I/opt/vc/include/interface/vmcs_host/linux/ -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/
LDFLAGS=-L/opt/vc/lib/ -L/media/videos/4tb/rpi/lib -lbcm_host -lc -lV3D2

OBJECTS=core.o mailbox.o
all: libGL.so test1
libGL.so: ${OBJECTS}
	${LD} -o $@ $+ -shared ${LDFLAGS}
	cp -v $@ /media/videos/4tb/rpi/lib/
test1: test1.o core.o mailbox.o
	${CXX} -o $@ $+ ${LDFLAGS}
core.o: core.c mailbox.h
mailbox.o: mailbox.cpp mailbox.h

%.o: %.c
	${CC} -c -o $@ $< ${CFLAGS}
