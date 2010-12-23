CFLAGS+=-I/usr/include
CFLAGS+=-Wall -Wextra -pedantic
CFLAGS+=-std=c99 `pkg-config --cflags x11` -DX11
LDFLAGS=-lreadline `pkg-config --libs tokyocabinet x11`

.PHONY: all clean
.SUFFIXES: .c

all: drop
	
drop: drop.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

.c:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o drop
