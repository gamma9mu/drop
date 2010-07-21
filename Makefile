INCL=-I/usr/include
LIBS=-lreadline `pkg-config --libs tokyocabinet`
#OPTS=-Wall -Wextra
OPTS=-Wall -Wextra -pedantic
ds: drop.c
	gcc $(OPTS) $(INCL) $(LIBS) -o drop drop.c
#	tcc $(OPTS) $(INCL) $(LIBS) -o drop drop.c
