
SRCS = producer.c msgd.c rabbit.c

OBJS = $(SRCS:.c=.o)
PRGS = $(basename $(SRCS))

CFLAGS = -g -Wextra -pedantic -std=c99 -L../../usr/lib -I../../usr/include
LINKS = -lrabbitmq -lczmq

.PHONY: run clean check-syntax


all: producer


producer: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LINKS) -o $@


%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@


run: $(PRGS)
	echo "run not yet implemented"

clean:
	rm -f  $(PRGS) $(OBJS)

check-syntax:
	$(CC) $(CFLAGS) -fsyntax-only $(SRCS)

