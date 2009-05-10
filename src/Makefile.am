
rootlibdir = $(exec_prefix)/$(libdir_name)
rootlib_LTLIBRARIES = \
	libdlo.la

include_HEADERS = \
	libdlo.h

libdlo_la_SOURCES = \
	dlo_base.h \
	dlo_data.h \
	dlo_defs.h \
	dlo_grfx.h \
	dlo_mode.h \
	dlo_structs.h \
	dlo_usb.h \
	dlo_grfx.c \
	dlo_mode.c \
	dlo_usb.c  \
	libdlo.c

libdlo_la_LDFLAGS = \
	-version-info $(LIBDLO_LT_CURRENT):$(LIBDLO_LT_REVISION):$(LIBDLO_LT_AGE) \
	-export-symbols exported_symbols

pkgconfigdir = $(prefix)/$(libdir_name)/pkgconfig
pkgconfig_DATA = libdlo.pc

EXTRA_DIST = \
	exported_symbols

# keep autotools from complaining there is no NEWS file
AUTOMAKE_OPTIONS = foreign

ACLOCAL_AMFLAGS = -I m4

