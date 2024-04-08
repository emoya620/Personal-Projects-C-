CC=gcc
CFLAGS=-g -O -pedantic -std=gnu17 -Wall -Werror -Wextra
LDFLAGS=-lcrypto -lm

.PHONY: all
all: fileSystem

fileSystem: fileSystem.o

.PHONY: clean
clean:
	rm -f *.o fileSystem
