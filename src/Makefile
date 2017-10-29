CC=gcc
CFLAGS=-O2 -m64 -mtune=generic -g
DEPS=
OBJ=token.o parse.o eval.o stream.o debug_parse.o debug_token.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

lox: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

all: lox

clean:
	rm *.o
