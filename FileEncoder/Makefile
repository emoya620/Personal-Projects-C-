CC=gcc
CFLAGS=-g -O -pedantic -std=gnu17 -Wall -Werror -Wextra
LDFLAGS=-lm -pthread

.PHONY: all
all: fileEncoder

fileEncoder: fileEncoder.o

.PHONY: clean
clean:
	rm -f *.o fileEncoder
