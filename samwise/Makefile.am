
AM_CFLAGS = -Werror -Wall -Wextra -pedantic -std=gnu99
AM_LDFLAGS = -lczmq -lrabbitmq

bin_PROGRAMS = sam

sam_SOURCES = \
	include/sam_logger.h  \
	src/sam_logger.c      \
	include/sam_log.h     \
	src/sam_log.c         \
\
	include/sam.h 				\
	include/sam_prelude.h \
	src/sam.c
