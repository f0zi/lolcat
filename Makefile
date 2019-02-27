
CC ?= gcc
LOLCAT_SRC ?= lolcat.c
CFLAGS ?= -std=c11 -Wall -Wextra -g -O3

DESTDIR ?= /usr/local/bin

all: lolcat

.PHONY: install clean

lolcat: $(LOLCAT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

install: lolcat
	install --strip lolcat $(DESTDIR)/lolcat

clean:
	rm -f lolcat

