
AM_CFLAGS = -g -Wall -Wextra -pedantic -std=gnu99
AM_LDFLAGS = -lczmq -lrabbitmq

bin_PROGRAMS = sam

sam_SOURCES =										\
	include/sam_logger.h					\
	src/sam_logger.c							\
	include/sam_log.h							\
	src/sam_log.c									\
\
	include/sam_msg.h							\
	include/sam_msg_rabbitmq.h		\
	src/sam_msg_rabbitmq.c				\
\
	include/sam.h									\
	include/sam_prelude.h					\
\
	include/playground.h					\
	src/playground_publish_loop.c	\
	src/sam.c
