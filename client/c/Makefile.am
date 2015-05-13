
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -g -Werror -Wall -Wextra -pedantic -std=gnu99


lib_LTLIBRARIES = lib/libsamwise.la
include_HEADERS = include/samwise.h


lib_libsamwise_la_SOURCES =  \
  include/samwise.h          \
  src/samwise.c

lib_libsamwise_la_LDFLAGS = \
  -lzmq                     \
  -lczmq


bin_PROGRAMS = samcli


samcli_SOURCES = src/samcli.c
samcli_LDADD = lib/libsamwise.la
samcli_LDFLAGS = -static
