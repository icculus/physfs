lib_LTLIBRARIES = libphysfs.la

libphysfsincludedir = $(includedir)
libphysfsinclude_HEADERS =	\
	physfs.h

if BUILD_MACOSX

ZLIB_FILES = zlib123/adler32.c		\
             zlib123/compress.c		\
             zlib123/crc32.c		\
             zlib123/crc32.h		\
             zlib123/deflate.c		\
             zlib123/deflate.h		\
             zlib123/gzio.c		\
             zlib123/infback.c		\
             zlib123/inffast.c		\
             zlib123/inffast.h		\
             zlib123/inffixed.h		\
             zlib123/inflate.c		\
             zlib123/inflate.h		\
             zlib123/inftrees.c		\
             zlib123/inftrees.h		\
             zlib123/trees.c		\
             zlib123/trees.h		\
             zlib123/uncompr.c		\
             zlib123/zconf.h		\
             zlib123/zlib.h		\
             zlib123/zutil.c		\
             zlib123/zutil.h


if BUILD_ZLIB
  ZLIB_SRC = $(ZLIB_FILES)
  ZLIB_INC = -I$(top_srcdir)/zlib123
  ZLIB_EXTRADIST =
else
  ZLIB_SRC =
  ZLIB_INC =
  ZLIB_EXTRADIST = $(ZLIB_FILES)
endif

libphysfs_la_SOURCES =	\
	physfs.c	        \
	physfs_internal.h	\
	physfs_byteorder.c  \
	archivers/dir.c		\
	archivers/grp.c		\
	archivers/wad.c		\
	archivers/hog.c		\
	archivers/mvl.c		\
	archivers/zip.c		\
	archivers/qpak.c   	\
	platform/unix.c     \
	platform/posix.c    \
    $(ZLIB_SRC)

libphysfs_la_INCLUDES = $(ZLIB_INC)

libphysfs_la_LDFLAGS = 		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)


if BUILD_TEST_PHYSFS
bin_PROGRAMS = test_physfs
test_physfs_INCLUDES = -I$(top_srcdir)
test_physfs_LDADD = $(top_srcdir)/libphysfs.la
test_physfs_SOURCES = test/test_physfs.c
TEST_EXTRADIST =
else
TEST_EXTRADIST = test/test_physfs.c
endif


EXTRA_DIST =	\
	CREDITS \
	LICENSE \
	CHANGELOG \
	INSTALL \
	TODO \
	Doxyfile \
	CWProjects.sit \
	physfs.spec \
	physfs.dsp \
	test_physfs.dsp \
	physfs_static.dsp \
	physfs.vcproj \
	test_physfs.vcproj \
	platform/skeleton.c    \
	platform/macclassic.c  \
	platform/win32.c       \
	platform/beos.cpp      \
	platform/os2.c         \
    extras/physfsrwops.h   \
    extras/physfsrwops.c   \
    extras/physfshttpd.c   \
	Makefile.am.oldautomake \
	Makefile.am.newautomake \
	zlib_license_change.txt \
    makeos2.cmd             \
    PBProjects             \
    $(ZLIB_EXTRADIST) $(BEOS_EXTRADIST) $(TEST_EXTRADIST)

else

SUBDIRS = platform archivers zlib123 . test extras

libphysfs_la_SOURCES =	\
	physfs.c	\
	physfs_internal.h	\
	physfs_byteorder.c

if BUILD_ZLIB
ZLIB_LIB = zlib123/libz.la
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
	PBProjects \
	CWProjects.sit \
	physfs.spec.in \
	physfs.spec \
	physfs.dsp \
	test_physfs.dsp \
	physfs_static.dsp \
	physfs.vcproj \
	test_physfs.vcproj \
	zlib_license_change.txt \
	makeos2.cmd


endif


dist-hook:
	perl -w -pi -e 'chomp; $$_ .= "\r\n";' $(distdir)/*.dsp $(distdir)/*.vcproj
	mkdir $(distdir)/docs
	echo "Docs are generated with the program "Doxygen" (http://www.doxygen.org/)," >> $(distdir)/docs/README
	echo " or can be read online at http://icculus.org/physfs/docs/" >> $(distdir)/docs/README
	echo >> $(distdir)/docs/README
	rm -rf `find $(distdir) -name "CVS" -type d`
	rm -rf `find $(distdir) -name ".svn" -type d`

