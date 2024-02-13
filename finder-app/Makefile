CC = $(CROSS_COMPILE)gcc
SRC = writer

all: writer

writer: writer.o
	$(CC) -o $(SRC) $(SRC).o

writer.o: writer.c
	$(CC) -c $(SRC).c

clean:
	rm -f *.o $(SRC)
