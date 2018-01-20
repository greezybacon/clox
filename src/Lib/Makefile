.PHONY: clean

CC=gcc
CFLAGS=-O2 -fPIC -m64 -mtune=generic -g
LDFLAGS=-shared
DEPS=
SOURCES=$(shell echo *.c)
OBJECTS=$(SOURCES:.c=.o)
TARGET=liblox.so

#%.o: %.c $(DEPS)
#	$(CC) -c -o $@ $< $(CFLAGS)

all: $(TARGET)

clean:
	rm $(OBJECTS) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
