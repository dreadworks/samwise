
AM_CFLAGS = -g -Wall -Wextra -pedantic -std=gnu99

#
#  LIBRARIES
#


#  libsam
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
#  BINARIES
#
bin_PROGRAMS = samd sam_selftest

#  samd
samd_SOURCES =   \
  include/samd.h \
  src/samd.c

samd_LDADD = src/libsam.la
samd_LDFLAGS = -static


#  selftest
sam_selftest_SOURCES = src/sam_selftest.c
sam_selftest_LDADD = src/libsam.la
sam_selftest_LDFLAGS = -static
