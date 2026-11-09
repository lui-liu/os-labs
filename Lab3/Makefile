CC = gcc
CFLAGS = -Wall -Wextra

all: parent child1 child2

parent: parent.c common.h
	$(CC) $(CFLAGS) -o parent parent.c

child1: child1.c common.h
	$(CC) $(CFLAGS) -o child1 child1.c

child2: child2.c common.h
	$(CC) $(CFLAGS) -o child2 child2.c

clean:
	rm -f parent child1 child2