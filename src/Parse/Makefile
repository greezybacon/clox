.PHONY: clean

CC=gcc
CFLAGS=-O2 -fPIC -m64 -mtune=generic -g
LDFLAGS=-L../Lib/ -llox
INC=-I ../
DEPS=
SOURCES=$(shell echo *.c)
OBJECTS=$(SOURCES:.c=.o)
TARGET=lox

%.o: %.c $(DEPS)
	$(CC) $(INC) -c -o $@ $< $(CFLAGS)

all: $(TARGET)

clean:
	rm $(OBJECTS) $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
