#!/usr/bin/make -f
CC = gcc
CFLAGS = -Wall -g

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
TARGET = ns-serial-mux

PREFIX ?= /usr
DESTDIR ?=

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(INC_DIRS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $< -o $@

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -D -m 644 ns-serial-mux.conf $(DESTDIR)/etc/ns-serial-mux/ns-serial-mux.conf

.PHONY: clean install

clean:
	rm -f $(TARGET) $(OBJECTS)