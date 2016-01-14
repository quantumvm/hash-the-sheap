CC=gcc
CFLAGS=-Wall -std=c99
SOURCES=heapdump.c

all:
	$(CC) $(CFLAGS) $(SOURCES) -o heapdump
clean:
	rm heapdump
install:
	install -m 0755 heapdump /usr/local/bin
