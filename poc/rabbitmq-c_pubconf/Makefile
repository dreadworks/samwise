
SRCS = rmq_pubconf-prod.c rmq_pubconf-cons.c

OBJS = $(SRCS:.c=.o)
PRGS = $(basename $(SRCS))

CFLAGS = -g -Wextra -pedantic -std=c99 -I./include -L./lib
LINKS = -lrabbitmq

.PHONY: run clean check-syntax

all: $(PRGS)


rmq_pubconf-prod: rmq_pubconf-prod.o util.o
	$(CC) $(CFLAGS) $< $(LINKS) -o $@

rmq_pubconf-cons: rmq_pubconf-cons.o util.o
	$(CC) $(CFLAGS) $< $(LINKS) -o $@


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

