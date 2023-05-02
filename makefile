CC = gcc
CFLAGS = -Wall

default: write

write: write.c
	$(CC) $(CFLAGS) write.c -lncurses -o write

debug: write.c
	$(CC) $(CFLAGS) -g write.c -lncurses -o write

