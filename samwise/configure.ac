#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT(samwise, 0.0.1, http://source.xing.com/felix-hamann/samwise/issues)
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AM_INIT_AUTOMAKE
LT_INIT
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CXX
AC_PROG_AWK
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
