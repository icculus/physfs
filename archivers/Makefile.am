noinst_LTLIBRARIES = libarchivers.la

if BUILD_ZLIB
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/zlib114
else
INCLUDES = -I$(top_srcdir)
endif

libarchivers_la_SOURCES =	\
	dir.c		\
	grp.c		\
	zip.c		\
	unzip.c     \
	unzip.h

