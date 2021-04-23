TARGET = bunny
SRC = src.c

CFLAGS += -O3 -static -s -std=c99 -Wall -Werror -pedantic
CPPFLAGS += -D_XOPEN_SOURCE=700

.PHONY: all clean install

all: bunny

bunny: bunny.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o bunny bunny.c

install: bunny
	install -m755 $(DESTDIR)/usr/sbin/bunny
	ln -frs $(DESTDIR)/usr/sbin/bunny $(DESTDIR)/usr/sbin/reboot
	ln -frs $(DESTDIR)/usr/sbin/bunny $(DESTDIR)/usr/sbin/shutdown
	mkdir -p $(DESTDIR)/etc/bunny
	mkdir -p $(DESTDIR)/etc/bunny.d
	# TODO install scripts here

clean:
	$(RM) bunny
