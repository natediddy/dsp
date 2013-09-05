CC = gcc
CFLAGS = -Wall -O2 -march=native -mtune=native
LDFLAGS = -lcurl

TARGET = dsp

prefix = /usr/local

SOURCES = dsp.c
OBJECTS = dsp.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

dsp.o: dsp.c
	$(CC) $(CFLAGS) -c dsp.c

clean:
	@rm -f $(OBJECTS)

clobber:
	@rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(prefix)/bin/$(TARGET)

uninstall:
	rm -f $(prefix)/bin/$(TARGET)

.PHONY: clean clobber install uninstall
