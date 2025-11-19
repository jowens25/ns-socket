CC = gcc
CFLAGS = -Wall -g


SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
TARGET = ns-socket-server


$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(INC_DIRS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INC_DIRS) -c $< -o $@

.PHONY: clean build_and_clean

clean:
	rm -f $(TARGET) $(OBJECTS)
