
AM_CFLAGS = -g -Wall -Wextra -pedantic -std=gnu99
AM_LDFLAGS = -lzmq -lczmq -lrabbitmq

bin_PROGRAMS = sam
# bin_PROGRAMS = samd

# samd_SOURCES =									\
#   include/sam.h									\
# 	src/samd.c

sam_SOURCES =										\
\
	include/sam_gen.h							\
	src/sam_gen.c									\
\
	include/sam_logger.h					\
	src/sam_logger.c							\
	include/sam_log.h							\
	src/sam_log.c									\
\
	include/sam_msg.h							\
	src/sam_msg.c									\
	include/sam_msg_rabbitmq.h		\
	src/sam_msg_rabbitmq.c				\
\
	include/sam.h									\
	include/sam_prelude.h					\
\
	include/playground.h					\
	src/playground_publish_loop.c	\
	src/sam.c
