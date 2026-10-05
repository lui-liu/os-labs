CC = gcc
CFLAGS = -Wall -Wextra

all: parent child1 child2

parent: parent.c
	$(CC) $(CFLAGS) -o parent parent.c

child1: child1.c
	$(CC) $(CFLAGS) -o child1 child1.c

child2: child2.c
	$(CC) $(CFLAGS) -o child2 child2.c

clean:
	rm -f parent child1 child2
