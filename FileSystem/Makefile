CC=gcc
CFLAGS=-g -O -pedantic -std=gnu17 -Wall -Werror -Wextra
LDFLAGS=-lcrypto -lm

.PHONY: all
all: nyufile

nyuenc: nyufile.o

nyuc.o: nyuc.c

.PHONY: clean
clean:
	rm -f *.o nyufile