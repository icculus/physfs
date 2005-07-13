X = adler32.c		\
	compress.c		\
	crc32.c			\
	crc32.h			\
	deflate.c		\
	deflate.h		\
	gzio.c			\
	infback.c		\
	inffast.c		\
	inffast.h		\
	inffixed.h		\
	inflate.c		\
	inflate.h		\
	inftrees.c		\
	inftrees.h		\
	trees.c			\
	trees.h			\
	uncompr.c		\
	zconf.h			\
	zlib.h			\
	zutil.c			\
	zutil.h

if BUILD_ZLIB
noinst_LTLIBRARIES = libz.la
libz_la_SOURCES = $(X)
endif

EXTRA_DIST = $(X)
