
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -g -Werror -Wall -Wextra -pedantic -std=gnu99


lib_LTLIBRARIES = src/libsamwise.la
include_HEADERS = include/samwise.h


src_libsamwise_la_SOURCES =  \
  include/samwise.h          \
  src/samwise.c

src_libsamwise_la_LDFLAGS = \
  -lzmq                     \
  -lczmq


bin_PROGRAMS = samcli


samcli_SOURCES = src/samcli.c
samcli_LDADD = src/libsamwise.la
samcli_LDFLAGS = -static
