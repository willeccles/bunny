TARGET = bunny
SRC = src.c

CFLAGS += -O3 -static -s -std=c99 -Wall -Werror -pedantic
CPPFLAGS += -D_XOPEN_SOURCE=700

.PHONY: all clean install

all: bunny

bunny: bunny.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o bunny bunny.c

install: bunny

clean:
	$(RM) bunny
