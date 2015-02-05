# Makefile
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a GNU GPLv3 license.

all: util.o dtls.o sctp.o ice.o sdp.o

util.o: util.c util.h
	gcc -c util.c

dtls.o: dtls.c dtls.h common.h util.h
	gcc -Wno-deprecated -c dtls.c `pkg-config --cflags openssl nice`

sctp.o: sctp.c sctp.h common.h util.h
	gcc -Wno-deprecated -DDEBUG_SCTP -c sctp.c `pkg-config --cflags openssl nice`

ice.o: ice.c ice.h common.h util.h
	gcc -Wno-deprecated -DDEBUG_SCTP -c ice.c `pkg-config --cflags openssl nice`

sdp.o: sdp.c sdp.h common.h util.h
	gcc -DDEBUG_SCTP -c sdp.c `pkg-config --cflags openssl nice`

clean:
	rm -f *.o *.dylib *.so *.a

