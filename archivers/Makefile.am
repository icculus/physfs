noinst_LTLIBRARIES = libarchivers.la

if BUILD_ZLIB
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/zlib123
else
INCLUDES = -I$(top_srcdir)
endif

libarchivers_la_SOURCES =	\
	dir.c		\
	grp.c		\
	wad.c		\
	hog.c		\
	mvl.c		\
	zip.c		\
	qpak.c

