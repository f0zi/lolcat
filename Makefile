
CXX ?= g++
LOLCAT_SRC ?= lolcat.cpp
CXXFLAGS ?= -std=c++11 -Wall -Wextra -g -O3

DESTDIR ?= /usr/local/bin

all: lolcat

.PHONY: install clean

lolcat: $(LOLCAT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

install: lolcat
	install --strip lolcat $(DESTDIR)/lolcat

clean:
	rm -f lolcat

