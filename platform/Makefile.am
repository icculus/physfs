noinst_LTLIBRARIES = libplatform.la

INCLUDES = -I$(top_srcdir)

if BUILD_BEOS_CPP
  X = beos.cpp
else
  Y = beos.cpp
endif

libplatform_la_SOURCES =	\
	unix.c                  \
	posix.c                 \
	$(X)

EXTRA_DIST =      \
	skeleton.c    \
	macclassic.c  \
	win32.c       \
	$(Y)

