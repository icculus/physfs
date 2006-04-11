noinst_LTLIBRARIES = libarchivers.la

INCLUDES = -I$(top_srcdir)

if BUILD_ZLIB
INCLUDES += -I$(top_srcdir)/zlib123
endif

if BUILD_LZMA
INCLUDES += -I$(top_srcdir)/lzma
endif

libarchivers_la_SOURCES =	\
	dir.c		\
	grp.c		\
	wad.c		\
	hog.c		\
	mvl.c		\
	zip.c		\
	lzma.c		\
	qpak.c		\
	mix.c
