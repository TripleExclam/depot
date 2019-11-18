.PHONY: all clean
.DEAFAULT: all

CFLAGS = -Wall -pthread -pedantic -Werror -g -std=gnu99
OBJECTS = 2310depot

all: $(OBJECTS)

2310depot: depot.c
	gcc $(CFLAGS) utilities.c depot.c -o 2310depot

clean:
	rm $(OBJECTS)
