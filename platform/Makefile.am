noinst_LTLIBRARIES = libplatform.la

INCLUDES = -I$(top_srcdir)

BEOS_SRC = @BUILD_BEOS_CPP@
BEOS_EXTRADIST = @BUILD_BEOS_EXTRA@

libplatform_la_SOURCES =	\
	unix.c                  \
	posix.c                 \
    $(BEOS_SRC)


EXTRA_DIST =      \
	skeleton.c    \
	macclassic.c  \
	win32.c       \
    $(BEOS_EXTRADIST)

