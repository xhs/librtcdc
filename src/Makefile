# Makefile
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a GNU GPLv3 license.

all: util.o dtls.o sctp.o ice.o sdp.o test.o
	gcc -o test *.o `pkg-config --libs openssl nice` -lusrsctp

util.o: util.c util.h
	gcc -c util.c

dtls.o: dtls.c dtls.h common.h util.h
	gcc -DINET -DINET6 -Wno-deprecated -c dtls.c `pkg-config --cflags openssl nice`

sctp.o: sctp.c sctp.h common.h util.h
	gcc -DINET -DINET6 -Wno-deprecated -DDEBUG_SCTP -c sctp.c `pkg-config --cflags openssl nice`

ice.o: ice.c ice.h common.h util.h
	gcc -DINET -DINET6 -Wno-deprecated -DDEBUG_SCTP -c ice.c `pkg-config --cflags openssl nice`

sdp.o: sdp.c sdp.h common.h util.h
	gcc -DINET -DINET6 -DDEBUG_SCTP -c sdp.c `pkg-config --cflags openssl nice`

test.o: test.c dtls.h sctp.h ice.h sdp.h
	gcc -DINET -DINET6 -DDEBUG_SCTP -c test.c `pkg-config --cflags openssl nice`

clean:
	rm -f test *.o *.dylib *.so *.a

