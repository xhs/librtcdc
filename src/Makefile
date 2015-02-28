# Makefile
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a GNU GPLv3 license.

CC=gcc
CFLAGS=-c -DINET -DINET6 -DDEBUG_SCTP -Wno-deprecated `pkg-config --cflags openssl nice`
LDFLAGS=`pkg-config --libs openssl nice` -lusrsctp -lpthread
SOURCES=util.c dtls.c sctp.c ice.c sdp.c dcep.c rtcdc.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=librtcdc.dylib

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) -shared -fPIC $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o *.so *.dylib *.a example 

