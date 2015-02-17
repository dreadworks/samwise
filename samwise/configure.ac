#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT(samwise, 0.0.1, http://source.xing.com/felix-hamann/samwise/issues)
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AM_INIT_AUTOMAKE
AC_PROG_CC

LT_INIT
# Checks for libraries.
# FIXME: Replace `main' with a function in `-lczmq':
AC_CHECK_LIB([czmq], [main])
# FIXME: Replace `main' with a function in `-lrabbitmq':
AC_CHECK_LIB([rabbitmq], [main])
# FIXME: Replace `main' with a function in `-lzmq':
AC_CHECK_LIB([zmq], [main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strdup])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
