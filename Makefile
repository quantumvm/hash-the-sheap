CC=gcc
CFLAGS=-Wall -std=c99

SOURCES=hashthesheap.c interface.c

CAIRO=$(shell pkg-config cairo --libs --cflags)
GTK3 =$(shell pkg-config gtk+-3.0 --libs --cflags)

LIBS =-lssl -lcrypto $(CAIRO) $(GTK3)

all:
	$(CC) $(CFLAGS) $(SOURCES) $(LIBS) -o hashthesheap
clean:
	rm hashthesheap
install:
	install -m 0755 hashthesheap /usr/local/bin
