CC=gcc

CFLAGS=-Wall -pedantic -Iinclude

.PHONY: clean compile
compile: angydev.so

angydev.so: obj/angydev.o
	$(CC) $(CFLAGS) -shared obj/angydev.o -o angydev.so

obj/angydev.o: src/angydev.c
	$(CC) $(CFLAGS) -fpic -c src/angydev.c -o obj/angydev.o