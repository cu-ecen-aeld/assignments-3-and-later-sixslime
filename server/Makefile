CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -O2

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

TARGET := aesdsocket

.PHONY: default all clean

default: all

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)
