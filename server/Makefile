CC ?= $(CROSS_COMPILE)gcc
TARGET = aesdsocket
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -pthread

default: all
	
all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) *.o $(TARGET)
