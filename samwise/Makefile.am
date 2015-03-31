
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -g -Werror -Wall -Wextra -pedantic -std=gnu99

#
#  LIBRARIES
#


#  libsam
lib_LTLIBRARIES = src/libsam.la
include_HEADERS = include/sam.h

src_libsam_la_SOURCES =          \
\
  include/sam_gen.h              \
  src/sam_gen.c                  \
\
  include/sam_log.h              \
  src/sam_log.c                  \
\
  include/sam_msg.h              \
  src/sam_msg.c                  \
\
  include/sam_cfg.h              \
  src/sam_cfg.c                  \
\
  include/sam_buf.h              \
  src/sam_buf.c                  \
\
  include/sam_be_rmq.h          \
  src/sam_be_rmq.c              \
\
  include/sam.h                  \
  include/sam_prelude.h          \
\
  src/sam.c

src_libsam_la_LDFLAGS = \
  -lzmq                 \
  -lczmq                \
  -ldb                  \
  -lrabbitmq


#
#  BINARIES
#
bin_PROGRAMS = samd samctl sam_selftest


#
#  samd
#
samd_SOURCES =   \
  include/samd.h \
  src/samd.c

samd_LDADD = src/libsam.la
samd_LDFLAGS = -static


#
#  samctl
#
samctl_SOURCES = \
	src/samctl.c   \
	src/sam_cfg.c  \
  src/sam_log.c

samctl_LDFLAGS = -lzmq -lczmq
samctl_CFLAGS = \
	$(AM_CFLAGS)  \
	-Wno-missing-field-initializers

#
#  selftest
#
sam_selftest_SOURCES =   \
  src/samd.c             \
  test/sam_log_test.c    \
  test/sam_gen_test.c    \
  test/sam_msg_test.c    \
  test/sam_cfg_test.c    \
  test/sam_buf_test.c    \
  test/sam_be_rmq_test.c \
  test/sam_test.c        \
\
  include/sam_selftest.h \
  src/sam_selftest.c

sam_selftest_LDADD = src/libsam.la
sam_selftest_LDFLAGS = -static -lcheck
sam_selftest_CFLAGS = -D__SAM_TEST
