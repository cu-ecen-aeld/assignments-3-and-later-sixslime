CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?= -lpthread -lrt
TARGET ?= aesdsocket

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

.PHONY: default all clean

default: all

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)
