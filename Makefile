include config.mk

CFLAGS += -g -DX11 -I/usr/include/readline -I/usr/include/ $(shell pkg-config --cflags x11)
LDFLAGS += -lreadline -ldl $(shell pkg-config --libs x11)

SOCFLAGS := -fPIC -shared
TCLDFLAGS := $(shell pkg-config --libs tokyocabinet)
DBMLDFLAGS := -lgdbm

.PHONY: all clean

SRC = drop.c db_util.c
OBJ = $(SRC:.c=.o)
DBS = db_gdbm.c db_tcbdb.c
DBO = $(DBS:.c=.so)

.c.o:
	$(CC) $(CFLAGS) -c $<

all: drop db_gdbm.so db_tcbdb.so

drop: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

db_gdbm.so: db_gdbm.c db.h
	$(CC) $(CFLAGS) $(SOCFLAGS) $(LDFLAGS) $(DBMLDFLAGS) -o $@ $<

db_tcbdb.so: db_tcbdb.c db.h
	$(CC) $(CFLAGS) $(SOCFLAGS) $(LDFLAGS) $(TCLDFLAGS) -o $@ $<

clean:
	rm -f *.{,s}o drop
