#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(samwise, 0.0.1, http://github.com/dreadworks/samwise/issues)
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AM_INIT_AUTOMAKE([subdir-objects])
LT_INIT
AC_CONFIG_MACRO_DIR([m4])
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lcheck':
AC_CHECK_LIB([check], [main])
# FIXME: Replace `main' with a function in `-lczmq':
AC_CHECK_LIB([czmq], [main])
# FIXME: Replace `main' with a function in `-ldb':
AC_CHECK_LIB([db], [main])
# FIXME: Replace `main' with a function in `-lrabbitmq':
AC_CHECK_LIB([rabbitmq], [main])
# FIXME: Replace `main' with a function in `-lzmq':
AC_CHECK_LIB([zmq], [main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset putenv])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
