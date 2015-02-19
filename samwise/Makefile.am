
AM_CFLAGS = -g -Wall -Wextra -pedantic -std=gnu99


#
#  libsam
#
lib_LTLIBRARIES = src/libsam.la
include_HEADERS = include/sam.h

src_libsam_la_SOURCES =					\
\
	include/sam_gen.h							\
	src/sam_gen.c									\
\
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

src_libsam_la_LDFLAGS = -lzmq -lczmq -lrabbitmq


#
#  samd
#
bin_PROGRAMS = samd
samd_SOURCES =   \
  include/samd.h \
  src/samd.c

samd_LDADD = src/libsam.la
samd_LDFLAGS = -static
