# Makefile
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a GNU GPLv3 license.

CC=gcc
CFLAGS=-c -DINET -DINET6 -DDEBUG_SCTP `pkg-config --cflags openssl nice`
CLIBS=`pkg-config --libs openssl nice` -lusrsctp

all: util.o dtls.o sctp.o ice.o sdp.o dcep.o test.o
	$(CC) -o test *.o $(CLIBS)

util.o: util.c util.h
	$(CC) $(CFLAGS) util.c

dtls.o: dtls.c dtls.h common.h util.h
	$(CC) $(CFLAGS) -Wno-deprecated dtls.c

sctp.o: sctp.c sctp.h common.h util.h
	$(CC) $(CFLAGS) -Wno-deprecated sctp.c

ice.o: ice.c ice.h common.h util.h
	$(CC) $(CFLAGS) -Wno-deprecated ice.c

sdp.o: sdp.c sdp.h common.h util.h
	$(CC) $(CFLAGS) sdp.c

test.o: test.c dtls.h sctp.h ice.h sdp.h
	$(CC) $(CFLAGS) test.c

dcep.o: dcep.c dcep.h common.h sctp.h
	$(CC) $(CFLAGS) dcep.c

clean:
	rm -f test *.o *.dylib *.so *.a

