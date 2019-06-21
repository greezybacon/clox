.PHONY: clean

CC=gcc
CFLAGS=-O2 -fPIC -m64 -mtune=native -g
LDFLAGS=
INC=-I ./
BDWGC=Vendor/bdwgc
DEPS=
SOURCES=$(shell echo Parse/*.c Eval/*.c Objects/*.c Lib/*.c Compile/*.c)
OBJECTS=$(SOURCES:.c=.o)
TARGET=lox

%.o: %.c $(DEPS)
	$(CC) $(INC) -c -o $@ $< $(CFLAGS)

all: $(TARGET)

clean:
	rm $(OBJECTS) $(TARGET)

bdwgc:
	$(MAKE) -C Vendor

lox.o: $(SOURCES)
standalone: lox.o bdwgc
	$(CC) $(CFLAGS) lox.o -o $(TARGET) $(LDFLAGS)

$(TARGET): $(OBJECTS) bdwgc
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS) $(BDWGC)/extra/gc.o
