CC = $(CROSS_COMPILE)gcc
TARGET = aesdsocket
CFLAGS  = -Wall -Werror

default: all
	
all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) *.o $(TARGET)
