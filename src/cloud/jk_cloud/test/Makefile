CC = gcc
CFLAGS = -Wall -Wextra -g -I..
LDFLAGS = 

SRCS = jk_websocket_linux.c test_ws.c
OBJS = $(SRCS:.c=.o)
TARGET = test_ws

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
