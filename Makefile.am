SUBDIRS=src  \
	test
dist_doc_DATA = README
ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = autogen.sh

MAINTAINERCLEANFILES = depcomp INSTALL install-sh missing aclocal.m4 config.guess config.sub configure

clean-local:
	test -d ./docs && rm -Rf ./docs || true

maintainer-clean-local:
	test -d ./m4 && rm -Rf ./m4 || true
	test -d ./config && rm -Rf ./config || true
	test -d ./autom4te.cache && rm -Rf ./autom4te.cache || true
	find . -name \*~ -exec rm {} \;
	find . -name \*.in -exec rm {} \;
	
