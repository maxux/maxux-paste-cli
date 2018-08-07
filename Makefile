EXEC = mpaste

CFLAGS  = -W -Wall -O2 -pipe -ansi -pedantic -std=gnu99
LDFLAGS = -lcurl -ljansson

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)


