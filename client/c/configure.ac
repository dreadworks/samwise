#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(samwise, 0.0.1, github.com/dreadworks/samwise/issues)
AC_CONFIG_SRCDIR([include/samwise.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AM_PROG_CC_C_O
AC_PROG_INSTALL
LT_INIT

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lczmq':
AC_CHECK_LIB([czmq], [main])
# FIXME: Replace `main' with a function in `-lzmq':
AC_CHECK_LIB([zmq], [main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_FUNC_MALLOC

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
