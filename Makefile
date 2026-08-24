CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -pedantic -g
SRCS    = $(wildcard src/*.c src/sched/*.c)
OBJS    = $(SRCS:.c=.o)
TARGET  = scheduler

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
