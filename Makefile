CC=gcc
CFLAGS=-Wall -std=c99
SOURCES=hashthesheap.c
LIB=-lssl -lcrypto

all:
	$(CC) $(CFLAGS) $(SOURCES) $(LIB) -o hashthesheap
clean:
	rm hashthesheap
install:
	install -m 0755 hashthesheap /usr/local/bin
