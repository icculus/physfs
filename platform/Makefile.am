noinst_LTLIBRARIES = libplatform.la

INCLUDES = -I$(top_srcdir)

libplatform_la_SOURCES =	\
	unix.c                  \
	posix.c                 \
    @BUILD_BEOS_CPP@


EXTRA_DIST =      \
	skeleton.c    \
	macclassic.c  \
	win32.c       \
    @BUILD_BEOS_EXTRA@

