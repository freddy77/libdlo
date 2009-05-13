#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([libdlo], [0.1.0], [libdlo@displaylink.com])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2])
AC_CONFIG_SRCDIR([src/dlo_grfx.c])
AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# create LIBTOOLS macros.
LT_INIT

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h usb.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
PKG_CHECK_MODULES(LIBUSB, libusb >= 0.1.12)
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday strchr])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 test/Makefile
                 test/test1/Makefile
                ])
AC_OUTPUT
AC_MSG_RESULT([
	libdlo $VERSION
	========
	
	prefix:			${prefix}
	exec_prefix:		${exec_prefix}
	libdir_name:		${libdir_name}
	mandir:			${mandir}
	includedir:		${includedir}
						
	compiler:		${CC}
	cflags:			${CFLAGS}
	ldflags:		${LDFLAGS}
								
	xsltproc:		${XSLTPROC}
])
											