lib_LTLIBRARIES = libphysfs.la

SUBDIRS = platform archivers zlib114 . test extras

libphysfsincludedir = $(includedir)
libphysfsinclude_HEADERS =	\
	physfs.h

libphysfs_la_SOURCES =	\
	physfs.c	\
	physfs_internal.h	\
	physfs_byteorder.c

if BUILD_ZLIB
ZLIB_LIB = zlib114/libz.la
else
ZLIB_LIB =
endif

libphysfs_la_LDFLAGS = 		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
libphysfs_la_LIBADD =	\
	archivers/libarchivers.la	\
	platform/libplatform.la	\
	$(ZLIB_LIB)

EXTRA_DIST =	\
	CREDITS \
	LICENSE \
	CHANGELOG \
	INSTALL \
	TODO \
	Doxyfile \
    docs

dist-hook:
	rm -rf `find $(distdir)/docs -name CVS`

