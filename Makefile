CFLAGS+=-O2 -Wall -Wextra -pedantic -std=c99 -DX11
SOCFLAGS := -fPIC -shared
MFLDFLAGS := -lreadline -ldl `pkg-config --libs x11`
TCLDFLAGS := `pkg-config --libs tokyocabinet`
DBMLDFLAGS := -lgdbm

.PHONY: all clean
.SUFFIXES: .c

all: drop db_gdbm.o db_tcbdb.so
	
drop: drop.o db_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(MFLDFLAGS) -o $@ $^

db_gdbm.o: db_gdbm.c db.h
	$(CC) $(CFLAGS) $(SOCFLAGS) $(LDFLAGS) $(DBMLDFLAGS) -o $@ $<

db_tcbdb.so: db_tcbdb.c db.h
	$(CC) $(CFLAGS) $(SOCFLAGS) $(LDFLAGS) $(TCLDFLAGS) -o $@ $<

.c:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.{,s}o drop
