noinst_LTLIBRARIES = libplatform.la

INCLUDES = -I$(top_srcdir)

if BUILD_BEOS
BEOS_SRC = beos.cpp
else
BEOS_SRC =
endif

if BUILD_BEOS
BEOS_EXTRADIST =
else
BEOS_EXTRADIST = beos.cpp
endif


libplatform_la_SOURCES =	\
	unix.c                  \
	posix.c                 \
    $(BEOS_SRC)


EXTRA_DIST =      \
	skeleton.c    \
	macclassic.c  \
	win32.c       \
    $(BEOS_EXTRADIST)

