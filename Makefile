INCL=-I/usr/include
LIBS=-L/usr/local/lib -ltokyocabinet -lz -lbz2 -lrt -lpthread -lm -lc -lreadline
OPTS=-Wall -Wextra -pedantic
ds: drop.c
	gcc $(OPTS) $(INCL) $(LIBS) -o drop drop.c
