noinst_LTLIBRARIES = libplatform.la

INCLUDES = -I$(top_srcdir)

X = beos.cpp
Y = beos.cpp

if BUILD_BEOS_CPP
  Y =
else
  X =
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

