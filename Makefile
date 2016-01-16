CC=gcc
CFLAGS=-Wall -std=c99
SOURCES=heapdump.c
LIB=-lssl -lcrypto

all:
	$(CC) $(CFLAGS) $(SOURCES) $(LIB) -o heapdump
clean:
	rm heapdump
install:
	install -m 0755 heapdump /usr/local/bin
