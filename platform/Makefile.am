noinst_LTLIBRARIES = libplatform.la

INCLUDES = -I$(top_srcdir)

if BUILD_BEOS
libplatform_la_SOURCES =	\
	posix.c                 \
    beos.cpp
else
libplatform_la_SOURCES =	\
	unix.c                  \
	win32.c                 \
	posix.c                 \
	os2.c
endif


EXTRA_DIST =      \
	skeleton.c    \
	macclassic.c  \
	win32.c       \
	pocketpc.c    \
	unix.c        \
	os2.c         \
    beos.cpp

